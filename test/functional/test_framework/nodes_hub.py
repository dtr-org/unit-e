#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from asyncio import (
    AbstractEventLoop,
    Protocol,
    coroutine,
    sleep as asyncio_sleep
)


class NodesHub:
    """
    A central hub to connect all the nodes at test/simulation time. It has many purposes:
      - Give us the ability to capture & analyze traffic
      - Give us the ability to add arbitrary delays/latencies between any node

    The hub will open many ports at the same time to handle inbound connections, one for each node. When a node A wants
    to send a message to node B, the message will travel through the hub (H hereinafter). So, if A wants to be connected
    to B and C, it will establish two connections to H (every open port of H will represent a specific node).

    In this class, we refer to the nodes through their index in the self.nodes property.
    """

    def __init__(self, loop, nodes, base_port, host='127.0.0.1'):
        self.loop = loop  # type: AbstractEventLoop
        self.nodes = nodes

        self.host = host
        self.base_port = base_port

        self.node2node_delays = {}

        self.proxy_tasks = []
        self.proxy_transports = {}

        self.pending_connection = None  # Lock-like object used by NodesHub.connect_nodes

        self.setup_proxies()

    def setup_proxies(self):
        """
        This method creates a listener proxy for each node, the connections from each proxy to the real node that they
        represent will be done whenever a node connects to the proxy.
        """

        for i, node in enumerate(self.nodes):
            fake_listener_coroutine = self.loop.create_server(
                protocol_factory=self.get_proxy_class(),
                host=self.host,
                port=self.get_proxy_port(i)
            )
            self.proxy_tasks.append(self.loop.create_task(fake_listener_coroutine))

    def get_proxy_class(self):
        """
        This method acts like a closure, allowing us to dynamically define anonymous classes at runtime.
        """

        hub_ref = self
        pending_connection = hub_ref.pending_connection  # We have to capture this reference in our closure

        class NodeProxy(Protocol):
            def connection_made(self, transport):
                # It will be the hub the responsible to send data, not this object
                hub_ref.proxy_transports[pending_connection] = transport

            def connection_lost(self, exc):
                # TODO: Should we do something here?
                pass

            def data_received(self, data):
                pass

            def eof_received(self):
                # TODO: Should we do something here?
                pass

        return NodeProxy

    def get_node_port(self, node_idx):
        return self.base_port + 2 * node_idx

    def get_proxy_port(self, node_idx):
        return self.base_port + 2 * node_idx + 1

    def get_proxy_address(self, node_idx):
        return '%s:%s' % (self.host, self.get_proxy_port(node_idx))

    def set_nodes_delay(self, src_idx, dst_idx, ms_delay):
        if ms_delay == 0:
            self.node2node_delays.pop((src_idx, dst_idx), None)
        else:
            self.node2node_delays[(src_idx, dst_idx)] = ms_delay

    @coroutine
    def connect_nodes(self, outbound_idx, inbound_idx):
        """
        :param outbound_idx: It refers to the "client" (the one asking for a new connection)
        :param inbound_idx: It refers to the "server" (the one listening for new connections)
        """

        # We have to wait until all the proxies are configured and listening
        while len(self.proxy_tasks) < len(self.nodes):
            yield from asyncio_sleep(0)

        # We have to be sure that all the previous calls to connect_nodes have finished. Because we are using
        # cooperative scheduling we don't have to worry about race conditions, this while loop is enough.
        while self.pending_connection is not None:
            yield from asyncio_sleep(0)

        # We acquire the lock. This tuple is also useful for the NodeProxy instance.
        pending_connection = (outbound_idx, inbound_idx)
        self.pending_connection = pending_connection

        if pending_connection in self.proxy_transports:
            self.pending_connection = None
            raise RuntimeError('Connection between node%s and node%s already established' % (outbound_idx, inbound_idx))

        client_node = self.nodes[outbound_idx]
        proxy_address = self.get_proxy_address(inbound_idx)

        client_node.addnode(proxy_address, 'add')     # Add the proxy to the outgoing connections list
        client_node.addnode(proxy_address, 'onetry')  # Connect to the proxy. Will trigger NodeProxy.connection_made

        # Here we wait until we are sure that NodeProxy.connection_made has been called.
        while pending_connection not in self.proxy_transports:
            yield from asyncio_sleep(0)

        # We release the lock
        self.pending_connection = None
