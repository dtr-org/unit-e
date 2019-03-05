#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests EmbargoMan on a network that has loops.
1) Transactions are being routed to the source
2) Source does not leak information about itself.
After source transaction returned, source node should behave as usual node
for example rebroadcast source transaction
"""

from test_framework.util import connect_nodes, connect_nodes_bi
from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import P2PInterface, network_thread_start
import time

# Number of loops in the network
LOOPS_N = 5

# Timeout for entire test duration
TEST_TIMEOUT = 60


class EmbargoManLoop(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2 + LOOPS_N

        self.extra_args = [['-debug=all',
                            '-embargotxs=1', '-embargomin=9999',
                            '-whitelist=127.0.0.1']] * self.num_nodes
        self.setup_clean_chain = True

    def setup_network(self):
        super().setup_nodes()

        # Creating LOOPS_N loops each looking as
        # 0 -> relay(1) <-> transit_node -> 0
        for i in range(LOOPS_N):
            transit_node = i + 2
            connect_nodes(self.nodes[0], 1)
            connect_nodes_bi(self.nodes, 1, transit_node)
            connect_nodes(self.nodes[transit_node], 0)

    def run_test(self):
        start_time = time.perf_counter()

        self.setup_stake_coins(self.nodes[0])

        for node in self.nodes:
            node.add_p2p_connection(InvRecorder())
        network_thread_start()

        # Exit IBD
        self.generate_sync(self.nodes[0])

        for node in self.nodes:
            node.p2p.wait_for_verack()

        address = self.nodes[0].getnewaddress("", "legacy")
        tx = self.nodes[0].sendtoaddress(address, 1)

        while True:
            problem_nodes = get_nodes_that_didnt_send_inv(self.nodes, tx)
            if len(problem_nodes) == 0:
                return

            if time.perf_counter() - start_time >= TEST_TIMEOUT:
                raise AssertionError("Nodes didn't send inv: %", problem_nodes)


def get_nodes_that_didnt_send_inv(all_nodes, tx):
    result = []
    for node in all_nodes:
        if tx not in node.p2p.invs:
            result.append(node.index)

    return result


class InvRecorder(P2PInterface):
    def __init__(self):
        super().__init__()
        self.invs = set()

    def on_inv(self, message):
        for inv in message.inv:
            self.invs.add("%064x" % inv.hash)


if __name__ == '__main__':
    EmbargoManLoop().main()
