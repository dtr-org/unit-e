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
    assert_finalizationstate,
    connect_nodes_bi,
    disconnect_nodes,
    json,
    sync_blocks,
    wait_until,
)

class FeatureReindexCommits(UnitETestFramework):

    def get_extra_args(self, reindex):
        finalization_params = json.dumps({
            'epochLength': 5,
            'minDepositSize': 1500,
            'dynastyLogoutDelay': 2,
            'withdrawalEpochDelay': 2
        })

        proposer_args = [
            '-proposing=1',
            '-esperanzaconfig=' +
            finalization_params]

        validator_args = [
            '-validating=1',
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
        self.assert_finalizer_status('NOT_VALIDATING')

        self.log.info("Setup deposit")
        self.generate_deposit()
        self.assert_finalizer_status('IS_VALIDATING')

        disconnect_nodes(self.proposer, self.finalizer.index)

        self.log.info("Generate few epochs")
        votes = self.generate_epoch(
            proposer=self.proposer,
            finalizer=self.finalizer,
            count=2)
        assert_equal(len(votes), 2)
        assert_equal(self.proposer.getblockcount(), 35)
        assert_finalizationstate(self.proposer,
                                 {'currentEpoch': 7,
                                  'lastJustifiedEpoch': 6,
                                  'lastFinalizedEpoch': 5,
                                  'validators': 1})

        self.log.info("Restart nodes, -reindex=1")
        self.restart_nodes(reindex=True)
        self.assert_finalizer_status('IS_VALIDATING')

        votes = self.generate_epoch(
            proposer=self.proposer,
            finalizer=self.finalizer,
            count=2)
        assert_equal(len(votes), 2)
        assert_equal(self.proposer.getblockcount(), 45)
        assert_finalizationstate(self.proposer,
                                 {'currentEpoch': 9,
                                  'lastJustifiedEpoch': 8,
                                  'lastFinalizedEpoch': 7,
                                  'validators': 1})



        self.log.info("Restart nodes, -reindex=0")
        self.restart_nodes(reindex=False)
        self.assert_finalizer_status('IS_VALIDATING')

        votes = self.generate_epoch(
            proposer=self.proposer,
            finalizer=self.finalizer,
            count=2)
        assert_equal(len(votes), 2)

        assert_equal(self.proposer.getblockcount(), 55)
        assert_finalizationstate(self.proposer,
                                 {'currentEpoch': 11,
                                  'lastJustifiedEpoch': 10,
                                  'lastFinalizedEpoch': 9,
                                  'validators': 1})

    def generate_deposit(self):
        deposit_tx = self.finalizer.deposit(
            self.finalizer.getnewaddress(
                "", "legacy"), 1500)
        self.wait_for_transaction(deposit_tx)

        self.proposer.generatetoaddress(
            24, self.proposer.getnewaddress(
                '', 'bech32'))
        assert_equal(self.proposer.getblockcount(), 25)

    def assert_finalizer_status(self, status):
        wait_until(lambda: self.finalizer.getvalidatorinfo()[
            'validator_status'] == status, timeout=10)

    def restart_nodes(self, reindex):
        tip_before = self.proposer.getbestblockhash()
        self.stop_nodes()
        self.start_nodes(self.get_extra_args(reindex))
        wait_until(lambda: self.proposer.getbestblockhash() == tip_before)


if __name__ == '__main__':
    FeatureReindexCommits().main()
