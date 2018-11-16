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
from collections import namedtuple


PendingConnection = namedtuple('PendingConnection', 'outbound_idx, inbound_idx')


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
        self.node2proxy_transports = {}

        self.relay_tasks = {}
        self.proxy2node_transports = {}

        self.pending_connection = None  # Lock-like object used by NodesHub.connect_nodes

        self.setup_proxies()

    def setup_proxies(self):
        """
        This method creates a listener proxy for each node, the connections from each proxy to the real node that they
        represent will be done whenever a node connects to the proxy.
        """

        for i, node in enumerate(self.nodes):
            proxy_coroutine = self.loop.create_server(
                protocol_factory=self.get_proxy_class(),
                host=self.host,
                port=self.get_proxy_port(i)
            )
            self.proxy_tasks.append(self.loop.create_task(proxy_coroutine))

    def get_proxy_class(self):
        """
        This method is a closure, allowing us to dynamically define anonymous classes at runtime for proxy objects
        """

        # Capturing references for our closure
        hub_ref = self
        pending_connection = hub_ref.pending_connection

        class NodeProxy(Protocol):
            def connection_made(self, transport):
                hub_ref.node2proxy_transports[pending_connection] = transport

            def connection_lost(self, exc):
                pass  # TODO: Should we do something here?

            def data_received(self, data):
                pass

            def eof_received(self):
                pass  # TODO: Should we do something here?

        return NodeProxy

    def get_proxy_relay_class(self):
        """
        This method is a closure, allowing us to dynamically define anonymous classes at runtime for proxy relay objects
        """

        hub_ref = self
        pending_connection = hub_ref.pending_connection

        class ProxyRelay(Protocol):
            def connection_made(self, transport):
                hub_ref.proxy2node_transports[pending_connection] = transport

            def connection_lost(self, exc):
                pass  # TODO: Should we do something here?

            def data_received(self, data):
                pass

            def eof_received(self):
                pass  # TODO: Should we do something here?

        return ProxyRelay

    def get_node_port(self, node_idx):
        return self.base_port + 2 * node_idx

    def get_proxy_port(self, node_idx):
        return self.base_port + 2 * node_idx + 1

    def get_proxy_address(self, node_idx):
        return '%s:%s' % (self.host, self.get_proxy_port(node_idx))

    def set_nodes_delay(self, outbound_idx, inbound_idx, ms_delay):
        if ms_delay == 0:
            self.node2node_delays.pop((outbound_idx, inbound_idx), None)
        else:
            self.node2node_delays[(outbound_idx, inbound_idx)] = ms_delay

    @coroutine
    def connect_nodes(self, outbound_idx: int, inbound_idx: int):
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
        pending_connection = PendingConnection(outbound_idx=outbound_idx, inbound_idx=inbound_idx)
        self.pending_connection = pending_connection

        if pending_connection in self.node2proxy_transports:
            self.pending_connection = None
            raise RuntimeError('Connection (node%s --> node%s) already established' % pending_connection)

        yield from self.connect_node_to_proxy(pending_connection)  # 1st step: connect "client" to the proxy
        yield from self.connect_proxy_to_node(pending_connection)  # 2nd step: connect proxy to its associated node

        # We release the lock
        self.pending_connection = None

    def connect_node_to_proxy(self, pending_connection: PendingConnection):
        """
        Establishes a connection between a real node and the proxy representing another node
        """
        client_node = self.nodes[pending_connection.outbound_idx]
        proxy_address = self.get_proxy_address(pending_connection.inbound_idx)

        client_node.addnode(proxy_address, 'add')  # Add the proxy to the outgoing connections list
        client_node.addnode(proxy_address, 'onetry')  # Connect to the proxy. Will trigger NodeProxy.connection_made

        # We wait until we are sure that NodeProxy.connection_made has been called.
        while pending_connection not in self.node2proxy_transports:
            yield from asyncio_sleep(0)

    def connect_proxy_to_node(self, pending_connection: PendingConnection):
        """
        Creates a client that connects to a node and relays messages between that node and its associated proxy
        """

        relay_coroutine = self.loop.create_connection(
            protocol_factory=self.get_proxy_relay_class(),
            host=self.host,
            port=self.get_node_port(pending_connection.inbound_idx)
        )
        self.relay_tasks[pending_connection] = self.loop.create_task(relay_coroutine)

        while pending_connection not in self.proxy2node_transports:
            yield from asyncio_sleep(0)
