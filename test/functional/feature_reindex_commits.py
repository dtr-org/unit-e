#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test running united with -reindex options and with finalization transactions
- Start a pair of nodes - validator and proposer
- Run a validator for some time
- Restart both nodes and check if finalization can continue with restarted state
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    disconnect_nodes,
    json,
    sync_blocks,
    wait_until,
)

MIN_DEPOSIT = 1500
EPOCH_LENGTH = 10


class FeatureReindexCommits(UnitETestFramework):

    def get_extra_args(self, reindex):
        finalization_params = json.dumps({
            'epochLength': EPOCH_LENGTH,
            'minDepositSize': MIN_DEPOSIT,
            'dynastyLogoutDelay': 2,
            'withdrawalEpochDelay': 2
        })
        proposer_args = ['-proposing=1', '-debug=all', '-whitelist=127.0.0.1',
                         '-esperanzaconfig=' + finalization_params]

        validator_args = [
            '-validating=1',
            '-debug=all',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' +
            finalization_params]

        if reindex:
            proposer_args.append('-reindex')
            validator_args.append('-reindex')

        return [proposer_args, validator_args]

    def set_test_params(self):
        self.setup_clean_chain = True
        self.extra_args = self.get_extra_args(False)
        self.num_nodes = len(self.extra_args)

    def run_test(self):
        self.proposer = self.nodes[0]
        self.finalizer = self.nodes[1]

        self.setup_stake_coins(self.proposer, self.finalizer)
        self.generate_sync(self.proposer)
        self.assert_validator_status('NOT_VALIDATING')

        self.generate_deposit()
        self.assert_validator_status('IS_VALIDATING')

        self.assert_votes(10)

        self.restart_nodes(True)
        self.assert_validator_status('IS_VALIDATING')

        self.generate_sync(self.proposer, EPOCH_LENGTH)

        self.restart_nodes(False)
        self.assert_validator_status('IS_VALIDATING')

        last_fin_epoch = self.finalizer.getfinalizationstate()[
            'lastFinalizedEpoch']
        self.assert_votes(10)
        assert_equal(
            last_fin_epoch + 10,
            self.finalizer.getfinalizationstate()['lastFinalizedEpoch'])

    def generate_deposit(self):
        deposit_tx = self.finalizer.deposit(
            self.finalizer.getnewaddress(
                "", "legacy"), MIN_DEPOSIT)
        self.wait_for_transaction(deposit_tx)

        self.proposer.generatetoaddress(
            51, self.proposer.getnewaddress(
                '', 'bech32'))
        assert_equal(self.proposer.getblockcount(), 52)

    def assert_validator_status(self, status):
        wait_until(lambda: self.finalizer.getvalidatorinfo()[
            'validator_status'] == status, timeout=10)

    def assert_votes(self, count):
        disconnect_nodes(self.proposer, self.finalizer.index)

        votes = self.generate_epoch(
            EPOCH_LENGTH,
            proposer=self.proposer,
            finalizer=self.finalizer,
            count=count)
        assert_equal(len(votes), count)

        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks(self.nodes)

    def restart_nodes(self, reindex):
        tip_before = self.proposer.getbestblockhash()

        self.stop_nodes()
        self.start_nodes(self.get_extra_args(reindex))

        connect_nodes_bi(self.nodes, 0, 1)
        wait_until(lambda: self.proposer.getbestblockhash() == tip_before)
        sync_blocks(self.nodes)


if __name__ == '__main__':
    FeatureReindexCommits().main()
