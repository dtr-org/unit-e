#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


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

    def __init__(self, nodes, base_port, ip_address='127.0.0.1'):
        self.nodes = nodes

        self.ip_address = ip_address  # We'll bind to this IP address
        self.base_port = base_port

        self.node2node_delays = {}

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

        # The "client" node will connect to the hub instead of directly connecting to the "server"
        self.nodes[outbound_idx].addnode(self.get_fake_node_address(inbound_idx), 'onetry')
