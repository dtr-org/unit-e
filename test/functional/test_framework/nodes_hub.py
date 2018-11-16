#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from asyncio import (
    AbstractEventLoop,
    Protocol,
    Transport,
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
    to B and C, it will establish two connections to H (every open port of H will represent a specific node), and H will
    establish one new connection to B, and another one to C.

    In this class, we refer to the nodes through their index in the self.nodes property.

    Even if in P2P protocols it makes no sense to use the distinction client-server, we'll use these terms here to
    distinguish between nodes listening for connections and nodes that proactively ask for a new connection.
    """

    def __init__(self, loop, nodes, base_port, host='127.0.0.1'):
        self.loop = loop  # type: AbstractEventLoop
        self.nodes = nodes

        self.host = host
        self.base_port = base_port

        self.node2node_delays = {}  # This allows us to specify asymmetric delays

        self.proxy_tasks = []
        self.client2proxy_transports = {}

        self.relay_tasks = {}
        self.proxy2server_transports = {}

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
        client2server_pair = hub_ref.pending_connection

        class NodeProxy(Protocol):
            def connection_made(self, transport: Transport):
                hub_ref.client2proxy_transports[client2server_pair] = transport

            def connection_lost(self, exc):
                pass  # TODO: Should we do something here?

            def data_received(self, data):
                hub_ref.loop.create_task(self.on_data_received(data))

            @coroutine
            def on_data_received(self, data):
                while client2server_pair not in hub_ref.proxy2server_transports:
                    yield from asyncio_sleep(0)  # We can't relay the data yet, we need a connection on the other side

                if client2server_pair in hub_ref.node2node_delays:
                    yield from asyncio_sleep(hub_ref.node2node_delays[client2server_pair])

                # TODO: override messages that contain information about IP addresses and ports
                proxy2server_transport = hub_ref.proxy2server_transports[client2server_pair]  # type: Transport
                proxy2server_transport.write(data)

            def eof_received(self):
                pass  # TODO: Should we do something here?

        return NodeProxy

    def get_proxy_relay_class(self):
        """
        This method is a closure, allowing us to dynamically define anonymous classes at runtime for proxy relay objects
        """

        hub_ref = self
        client2server_pair = hub_ref.pending_connection
        server2client_pair = client2server_pair[::-1]

        class ProxyRelay(Protocol):
            def connection_made(self, transport: Transport):
                hub_ref.proxy2server_transports[client2server_pair] = transport

            def connection_lost(self, exc):
                pass  # TODO: Should we do something here?

            def data_received(self, data):
                hub_ref.loop.create_task(self.on_data_received(data))

            @coroutine
            def on_data_received(self, data):
                while client2server_pair not in hub_ref.client2proxy_transports:
                    yield from asyncio_sleep(0)  # We can't relay the data yet, we need a connection on the other side

                if server2client_pair in hub_ref.node2node_delays:
                    yield from asyncio_sleep(hub_ref.node2node_delays[server2client_pair])

                # TODO: override messages that contain information about IP addresses and ports
                client2proxy_transport = hub_ref.client2proxy_transports[client2server_pair]  # type: Transport
                client2proxy_transport.write(data)

            def eof_received(self):
                pass  # TODO: Should we do something here?

        return ProxyRelay

    def get_node_port(self, node_idx):
        return self.base_port + 2 * node_idx

    def get_proxy_port(self, node_idx):
        return self.base_port + 2 * node_idx + 1

    def get_proxy_address(self, node_idx):
        return '%s:%s' % (self.host, self.get_proxy_port(node_idx))

    def set_nodes_delay(self, outbound_idx, inbound_idx, delay):
        if delay == 0:
            self.node2node_delays.pop((outbound_idx, inbound_idx), None)
        else:
            self.node2node_delays[(outbound_idx, inbound_idx)] = delay  # delay is measured in seconds

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
        self.pending_connection = (outbound_idx, inbound_idx)

        if (
                self.pending_connection in self.client2proxy_transports or
                self.pending_connection in self.proxy2server_transports
        ):
            raise RuntimeError('Connection (node%s --> node%s) already established' % self.pending_connection[:])

        self.connect_client_to_proxy(*self.pending_connection)  # 1st step: connect "client" to the proxy
        self.connect_proxy_to_server(*self.pending_connection)  # 2nd step: connect proxy 2 its associated node

        # We wait until we know that all the connections have been properly created
        while (
                self.pending_connection not in self.client2proxy_transports or
                self.pending_connection not in self.proxy2server_transports
        ):
            yield from asyncio_sleep(0)

        self.pending_connection = None  # We release the lock

    def connect_client_to_proxy(self, outbound_idx, inbound_idx):
        """
        Establishes a connection between a real node and the proxy representing another node
        """
        client_node = self.nodes[outbound_idx]
        proxy_address = self.get_proxy_address(inbound_idx)

        client_node.addnode(proxy_address, 'add')     # Add the proxy to the outgoing connections list
        client_node.addnode(proxy_address, 'onetry')  # Connect to the proxy. Will trigger NodeProxy.connection_made

    def connect_proxy_to_server(self, outbound_idx, inbound_idx):
        """
        Creates a client that connects to a node and relays messages between that node and its associated proxy
        """

        relay_coroutine = self.loop.create_connection(
            protocol_factory=self.get_proxy_relay_class(),
            host=self.host,
            port=self.get_node_port(inbound_idx)
        )
        self.relay_tasks[(outbound_idx, inbound_idx)] = self.loop.create_task(relay_coroutine)
