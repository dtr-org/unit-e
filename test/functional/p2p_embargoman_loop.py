#!/usr/bin/env python3
# Copyright(c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests EmbargoMan on a network that has loops.
1) Transactions are being routed to the source and embargo timer is not expired.

2) Source does not leak information about itself.
After source transaction returned, source node should behave as usual node
for example rebroadcast source transaction
"""

from test_framework.util import connect_nodes, connect_nodes_bi
from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import P2PInterface, network_thread_start
import time
import os.path
import re

# Number of loops in the network
LOOPS_N = 5


class EmbargoManLoop(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2 + LOOPS_N

        self.extra_args = [['-proposing=1', '-debug=all',
                            '-embargotxs=1']] * self.num_nodes
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
        self.nodes[0].importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        for node in self.nodes:
            node.add_p2p_connection(InvRecorder())
        network_thread_start()

        # Exit IBD
        self.nodes[0].generate(1)
        self.sync_all()

        for node in self.nodes:
            node.p2p.wait_for_verack()

        attempts = 3
        for attempt in range(attempts):
            try:
                self.nodes[0].generate(1)
                self.sync_all()

                address = self.nodes[0].getnewaddress("", "legacy")
                tx = self.nodes[0].sendtoaddress(address, 1)

                if not wait_for_debug_log_pattern(self.nodes[0],
                                                  "Embargo is lifted for tx: %s"
                                                  % tx):
                    raise AssertionError("Embargo was not lifted")

                # Give time to everything to propagate
                time.sleep(20)

                for i in range(1, self.num_nodes):
                    assert (tx in self.nodes[i].p2p.invs)

                if tx not in self.nodes[0].p2p.invs:
                    raise AssertionError("Node 0 didn't send inv => it is the "
                                         "source of %s" % tx)
                break
            except AssertionError:
                if attempt == attempts - 1:
                    raise


class InvRecorder(P2PInterface):
    def __init__(self):
        super().__init__()
        self.invs = set()

    def on_inv(self, message):
        for inv in message.inv:
            self.invs.add("%064x" % inv.hash)


def get_debug_log_path(node):
    return os.path.join(node.datadir, "regtest", "debug.log")


def debug_log_contains(node, pattern):
    regex = re.compile(pattern)
    with open(get_debug_log_path(node)) as f:
        for line in f.readlines():
            if regex.search(line) is not None:
                return True
    return False


def wait_for_debug_log_pattern(node, pattern, timeout_s=30):
    now = time.perf_counter()
    while True:
        if debug_log_contains(node, pattern):
            return True
        time.sleep(0.5)
        if time.perf_counter() - now > timeout_s:
            return False


if __name__ == '__main__':
    EmbargoManLoop().main()
