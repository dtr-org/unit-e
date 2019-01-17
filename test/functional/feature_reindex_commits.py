#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test running united with -reindex options and with finalization transactions

- Start a pair of nodes - validator and proposer
- Run a validator for some time
- Restart both nodes and check if finalization can continue with restarted state
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import wait_until, sync_blocks, json, assert_equal, \
    connect_nodes_bi
from test_framework.admin import Admin
import os
import re

MIN_DEPOSIT = 1500
EPOCH_LENGTH = 10


def get_extra_args(reindex):
    finalization_params = json.dumps({
        'epochLength': EPOCH_LENGTH,
        'minDepositSize': MIN_DEPOSIT,
        'dynastyLogoutDelay': 2,
        'withdrawalEpochDelay': 2
    })
    proposer_args = ['-proposing=1', '-validating=0',
                     '-debug=all', '-whitelist=127.0.0.1',
                     '-esperanzaconfig=' + finalization_params]

    validator_args = ['-proposing=0', '-validating=1',
                      '-debug=all', '-whitelist=127.0.0.1',
                      '-esperanzaconfig=' + finalization_params]
    if reindex:
        proposer_args.append('-reindex')
        validator_args.append('-reindex')

    return [proposer_args, validator_args]


class ReindexCommits(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.extra_args = get_extra_args(False)
        self.num_nodes = len(self.extra_args)

    def run_test(self):
        proposer = self.nodes[0]
        validator = self.nodes[1]

        proposer.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        validator.importmasterkey(
            'chef gas expect never jump rebel huge rabbit venue nature dwarf '
            'pact below surprise foam magnet science sister shrimp blanket '
            'example okay office ugly')

        # Exit IBD
        self.generate_sync(proposer)

        Admin.authorize_and_disable(self, proposer)

        val_address = validator.getnewaddress("", "legacy")

        deposit_tx = validator.deposit(val_address, MIN_DEPOSIT)
        self.wait_for_transaction(deposit_tx)

        self.generate_sync(proposer, EPOCH_LENGTH * 10)

        assert_log_does_not_contain_errors(validator)
        self.restart_nodes(True)
        assert_log_does_not_contain_errors(validator)

        self.generate_sync(proposer, EPOCH_LENGTH)

        assert_log_does_not_contain_errors(validator)
        self.restart_nodes(False)
        assert_log_does_not_contain_errors(validator)

        last_fin_epoch = validator.getfinalizationstate()['lastFinalizedEpoch']
        self.generate_sync(proposer, 10 * EPOCH_LENGTH)
        assert_equal(last_fin_epoch + 10,
                     validator.getfinalizationstate()['lastFinalizedEpoch'])

    def restart_nodes(self, reindex):
        node0 = self.nodes[0]
        block_count_before = node0.getblockcount()

        self.stop_nodes()
        self.start_nodes(get_extra_args(reindex))

        for i in range(self.num_nodes - 1):
            connect_nodes_bi(self.nodes, i, i + 1)

        wait_until(lambda: node0.getblockcount() == block_count_before)

        sync_blocks(self.nodes)


def assert_log_does_not_contain_errors(node):
    path = os.path.join(node.datadir, "regtest", "debug.log")
    compiled_re = re.compile("error:", re.IGNORECASE)

    with open(path) as f:
        for line in f.readlines():
            if compiled_re.search(line):
                raise AssertionError(
                    "Log of node %d contains stop-pattern: %s" % (
                        node.index, line))


if __name__ == '__main__':
    ReindexCommits().main()
