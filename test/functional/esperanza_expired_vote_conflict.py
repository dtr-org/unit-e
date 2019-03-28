#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.mininode import P2PInterface, network_thread_start
from test_framework.test_framework import UnitETestFramework
from test_framework.util import json, sync_blocks, assert_equal, wait_until
import os.path
import re

"""
This test checks validator behavior in case of huge vote propagation
delays. We model this by blocking vote propagation completely.
Blocks will be propagated as usual.
We expect that despite the blockage validator's internal state will remain
consistent and it will be able to finalize again as soon as the opportunity
arises
"""

MIN_DEPOSIT_SIZE = 1000
EPOCH_LENGTH = 10


class ExpiredVoteConflict(UnitETestFramework):
    def set_test_params(self):
        finalization_params = json.dumps({
            'epochLength': EPOCH_LENGTH,
            'minDepositSize': MIN_DEPOSIT_SIZE,
        })

        self.extra_args = [
            ['-debug=all', '-whitelist=127.0.0.1',
             '-esperanzaconfig=' + finalization_params],

            ['-validating=1', '-debug=all',
             '-whitelist=127.0.0.1', '-esperanzaconfig=' + finalization_params],
        ]

        self.num_nodes = len(self.extra_args)

        self.setup_clean_chain = True

    def setup_network(self):
        super().setup_nodes()
        # We do not connect nodes here. MiniRelay will be used instead

    def run_test(self):
        self.proposer = self.nodes[0]
        self.validator = self.nodes[1]
        proposer = self.proposer
        validator = self.validator

        self.setup_stake_coins(proposer, validator)

        relay = MiniRelay()
        relay.relay_txs = True
        relay.connect_nodes(proposer, validator)

        network_thread_start()
        relay.wait_for_verack()

        # Exit IBD
        self.generate_sync(proposer)

        deposit = validator.deposit(
            validator.getnewaddress("", "legacy"), MIN_DEPOSIT_SIZE)

        self.wait_for_transaction(deposit, timeout=30)

        # Later in this test we will be operating in 'epochs', not blocks
        # So it is convenient to always start and end at checkpoint
        # -1 for IBD
        # -1 to be at checkpoint
        self.generate_sync(proposer, EPOCH_LENGTH - 2)

        # Enable tx censorship
        relay.relay_txs = False

        # Checking behaviour when votes are casted on top of deposit
        self.generate_epochs_without_mempool_sync(6)
        # First few epochs should be insta-finalized, but after that
        # finalization should not happen since validator is detached
        assert_equal(3, self.last_finalized_epoch)

        # Validator is back, finalization should now work
        relay.relay_txs = True
        self.generate_epochs(4)
        assert_equal(8, self.last_finalized_epoch)

        # Checking behaviour when votes are casted on top of other votes
        relay.relay_txs = False
        self.generate_epochs_without_mempool_sync(4)
        assert_equal(8, self.last_finalized_epoch)
        relay.relay_txs = True
        self.generate_epochs(4)
        assert_equal(16, self.last_finalized_epoch)

        # Checking behaviour when votes are casted on top of logout
        logout = validator.logout()
        self.wait_for_transaction(logout, timeout=30)

        relay.relay_txs = False
        self.generate_epochs_without_mempool_sync(10)
        assert_equal(16, self.last_finalized_epoch)

        assert_log_does_not_contain(validator, "error")

    @property
    def last_finalized_epoch(self):
        return self.validator.getfinalizationstate()['lastFinalizedEpoch']

    def generate_epochs_without_mempool_sync(self, n_epochs):
        for _ in range(EPOCH_LENGTH * n_epochs):
            self.proposer.generate(1)
            sync_blocks(self.nodes)
            self.validator.syncwithvalidationinterfacequeue()

    def generate_epochs(self, n_epochs):
        self.generate_sync(self.proposer, EPOCH_LENGTH * n_epochs)


# Connects 2 normal nodes through mininodes and acts as a p2p relay for all
# traffic between nodes. Can disable transactions relay while relaying blocks
class MiniRelay:
    class Node(P2PInterface):
        def __init__(self):
            super().__init__()
            self.send_to = None
            self.relay_txs = True
            self.verack_received = False

        def on_data(self, command, raw_message):
            if not self.relay_txs and command == b'tx':
                return

            if command == b'verack':
                self.verack_received = True

            self.send_to.send_data(command, raw_message)

        def wait_for_verack(self):
            wait_until(lambda: self.verack_received, timeout=30)

    def __init__(self):
        super().__init__()
        self.mininode1 = MiniRelay.Node()
        self.mininode2 = MiniRelay.Node()

        self.mininode1.send_to = self.mininode2
        self.mininode2.send_to = self.mininode1

        self.relay_txs = True

    @property
    def relay_txs(self):
        return self.mininode1.relay_txs

    @relay_txs.setter
    def relay_txs(self, value):
        self.mininode1.relay_txs = value
        self.mininode2.relay_txs = value

    def wait_for_verack(self):
        self.mininode1.wait_for_verack()
        self.mininode2.wait_for_verack()

    def connect_nodes(self, node1, node2):
        node1.add_p2p_connection(self.mininode1)
        node2.add_p2p_connection(self.mininode2)


def assert_log_does_not_contain(node, stop_pattern, skip_first_n_lines=500):
    path = os.path.join(node.datadir, "regtest", "debug.log")
    compiled_re = re.compile(stop_pattern, re.IGNORECASE)

    i = 0
    with open(path) as f:
        for line in f.readlines():
            i += 1
            if i < skip_first_n_lines:
                continue
            if compiled_re.search(line):
                raise AssertionError(
                    "Log of node %d contains stop-pattern: %s" % (
                        node.index, line))


if __name__ == '__main__':
    ExpiredVoteConflict().main()
