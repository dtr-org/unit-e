#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from asyncio import AbstractEventLoop, Protocol


class NodesHub:
    """
    A central hub to connect all the nodes at test/simulation time. It has many purposes:
      - Give us the ability to capture traffic
      - Give us the ability to add arbitrary delays/latencies between any node

    The hub will open many ports at the same time to handle inbound connections, one for each node. When a node A wants
    to send a message to node B, the message will travel through the hub (H hereinafter). So, if A wants to be connected
    to B and C, it will establish two connections to H (every open port of H will represent a specific node).

    In this class, we refer to the nodes through their index in the self.nodes property.
    """

    def __init__(self, loop, nodes, base_port, ip_address='127.0.0.1'):
        self.loop = loop  # type: AbstractEventLoop
        self.nodes = nodes

        self.ip_address = ip_address  # We'll bind to this IP address
        self.base_port = base_port

        self.node2node_delays = {}

        self.fake_listener_tasks = []
        self.setup_fake_listeners()

    def setup_fake_listeners(self):
        for i, node in enumerate(self.nodes):
            fake_listener_coroutine = self.loop.create_server(
                protocol_factory=self.get_fake_listener_class(i),
                host=self.ip_address,
                port=self.get_fake_node_port(i)
            )
            self.fake_listener_tasks.append(self.loop.create_task(fake_listener_coroutine))

    def get_fake_listener_class(self, node_idx):
        """
        This method acts like a closure, allowing us to dynamically define anonymous classes at runtime.
        """

        hub_ref = self

        class FakeListener(Protocol):
            def connection_made(self, transport):
                pass

            def connection_lost(self, exc):
                pass

            def data_received(self, data):
                pass

            def eof_received(self):
                pass

        return FakeListener

    def get_real_node_port(self, node_idx):
        return self.base_port + 2 * node_idx

    def get_fake_node_port(self, node_idx):
        return self.base_port + 2 * node_idx + 1

    def get_fake_node_address(self, node_idx):
        return '%s:%s' % (self.ip_address, self.get_fake_node_port(node_idx))

    def set_nodes_delay(self, src_idx, dst_idx, ms_delay):
        if ms_delay == 0:
            self.node2node_delays.pop((src_idx, dst_idx), None)
        else:
            self.node2node_delays[(src_idx, dst_idx)] = ms_delay

    def connect_nodes(self, outbound_idx, inbound_idx):
        """
        :param outbound_idx: It refers to the "client"
        :param inbound_idx: It refers to the "server"
        """

        # TODO: Connect the hub to the "server" node
        # TODO: Check which is the proper connection order (we have to take care of the "handshaking" protocol

        client_node = self.nodes[outbound_idx]
        server_address = self.get_fake_node_address(inbound_idx)

        client_node.addnode(server_address, 'add')     # Add the "server" to the outgoing connections list
        client_node.addnode(server_address, 'onetry')  # Establish connection to the server
