#!/usr/bin/env python3
# Copyright(c) 2018 The unit - e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http: // www.opensource.org/licenses/mit-license.php.
"""
Tests EmbargoMan on a network that has a shape of a star.
Star topology ensures that transactions are not propagating - all nodes
except the star center are conceptually black holes

This test ensures that under EmbargoMan:
1) Transactions are sent to only one peer. They are embargoed and no
other nodes have this transaction until embargo timeout
2) After some time all transactions should fluff => all nodes have them
3) Relay changes after too many transactions were sent to a black hole
"""

from test_framework.util import connect_nodes, assert_equal
from test_framework.test_framework import UnitETestFramework
import time

# Number of transactions to send in this test
TRANSACTIONS_N = 5

# Minimum embargo to use in this test
EMBARGO_SECONDS = 20


class EmbargoManStar(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4

        self.extra_args = [['-proposing=1',
                            '-debug=all',
                            '-whitelist=127.0.0.1',
                            '-embargotxs=1',
                            '-embargomin=%s' % EMBARGO_SECONDS,
                            '-embargoavgadd=0']] * self.num_nodes
        self.setup_clean_chain = True

    def setup_network(self):
        super().setup_nodes()

        # Creating star-like topology with node 0 in the middle
        for node in range(1, self.num_nodes):
            connect_nodes(self.nodes[0], node)

    def run_test(self):
        self.nodes[0].importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        # Exit IBD
        self.generate_sync(self.nodes[0])

        relay1 = self.single_run()
        relay2 = self.single_run()

        # Relay should change because txs were fluffed too often
        assert (relay1 != relay2)

    def single_run(self):
        run_start_time = time.perf_counter()
        txs = []
        for _ in range(TRANSACTIONS_N):
            address = self.nodes[0].getnewaddress("", "legacy")
            tx = self.nodes[0].sendtoaddress(address, 1)
            txs.append(tx)

        # Minimum embargo is EMBARGO_SECONDS seconds, we want to
        # ensure that during this time only relay has txs. We also wait a
        # little less than embargo to account for different synchronization
        # issues
        nodes_with_txs_before_fluff = set()
        while True:
            presence = self.collect_tx_presence(txs)
            if time.perf_counter() >= run_start_time + EMBARGO_SECONDS - 1:
                break
            nodes_with_txs_before_fluff |= presence

        # Check that only one node has tx(relay)
        assert_equal(1, len(nodes_with_txs_before_fluff))

        # Now we want to ensure that fluff happened and all nodes receive
        # transactions. Do not want to exceed a minute for the whole run
        supposed_end_time = run_start_time + 60

        for tx in txs:
            timeout = supposed_end_time - time.perf_counter()
            self.wait_for_transaction(tx, timeout=timeout)

        return next(iter(nodes_with_txs_before_fluff))

    def collect_tx_presence(self, txs):
        leafs = range(1, self.num_nodes)
        presence = set()
        for tx in txs:
            for leaf in leafs:
                if has_tx(self.nodes[leaf], tx):
                    presence.add(leaf)
        return presence


def has_tx(node, tx):
    try:
        node.getrawtransaction(tx)
        return True
    except:
        return False


if __name__ == '__main__':
    EmbargoManStar().main()
