#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import (
    json,
    connect_nodes,
    assert_finalizationstate,
    sync_blocks,
    disconnect_nodes,
    sync_mempools
)
from test_framework.util import assert_equal
from test_framework.test_framework import UnitETestFramework


class EsperanzaLogoutTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 3

        params_data = {
            'epochLength': 10,
            'dynastyLogoutDelay': 3
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = [ '-esperanzaconfig=' + json_params]

        self.extra_args = [proposer_node_params,
                           validator_node_params,
                           validator_node_params]
        self.setup_clean_chain = True

    # create topology where arrows denote non-persistent connection
    # finalizer1 → proposer ← finalizer2
    def setup_network(self):
        self.setup_nodes()

        proposer = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]

        connect_nodes(finalizer1, proposer.index)
        connect_nodes(finalizer2, proposer.index)

    def run_test(self):

        proposer = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]

        self.setup_stake_coins(*self.nodes)

        # Leave IBD
        proposer.generate(1)
        sync_blocks([finalizer1, finalizer2])

        deptx_1 = finalizer1.deposit(finalizer1.getnewaddress("", "legacy"), 1500)
        deptx_2 = finalizer2.deposit(finalizer2.getnewaddress("", "legacy"), 3001)

        # wait for deposits to propagate
        self.wait_for_transaction(deptx_1, 60)
        self.wait_for_transaction(deptx_2, 60)

        assert_finalizationstate(proposer, {'currentEpoch': 0,
                                            'currentDynasty': 0,
                                            'lastJustifiedEpoch': 0,
                                            'lastFinalizedEpoch': 0,
                                            'validators': 0})

        disconnect_nodes(finalizer1, proposer.index)
        disconnect_nodes(finalizer2, proposer.index)

        # Generate enough blocks to advance 2 dynasties
        proposer.generate(39)  # 5 * 10 - 1 (from IBD)
        assert_equal(proposer.getblockcount(), 40)

        # Check that validators are not yet included
        assert_finalizationstate(proposer, {'currentEpoch': 4,
                                            'currentDynasty': 2,
                                            'validators': 0})

        # Advance another dynasty to make the validators active
        proposer.generate(10)
        assert_equal(proposer.getblockcount(), 50)
        assert_finalizationstate(proposer, {'currentEpoch': 5,
                                            'currentDynasty': 3,
                                            'lastJustifiedEpoch': 4,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 2})

        connect_nodes(proposer, finalizer1.index)
        connect_nodes(proposer, finalizer2.index)

        sync_blocks([proposer, finalizer1, finalizer2])
        sync_mempools([proposer, finalizer1, finalizer2])

        # Mine the votes to avoid the next logout to conflict with a vote in the mempool
        proposer.generate(1)
        assert_equal(proposer.getblockcount(), 51)
        sync_blocks([proposer, finalizer1, finalizer2])

        # Logout included in epoch 5, logout will be effective in dynasty 3+3 = 6
        logout_tx = finalizer1.logout()
        self.wait_for_transaction(logout_tx, 60)

        disconnect_nodes(finalizer1, proposer.index)

        # Check that the validator is still voting for epoch 6
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 60)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        # Here we miss finalization because the finalizers need to be in curDyn and prevDyn
        # in order for their votes to count for the finalization
        assert_finalizationstate(proposer, {'currentEpoch': 6,
                                            'currentDynasty': 4,
                                            'lastJustifiedEpoch': 5,
                                            'lastFinalizedEpoch': 4,
                                            'validators': 2})

        resp = finalizer1.getvalidatorinfo()
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        # Check that the validator is still voting for epoch 7
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 70)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        # Finalization recovers again to optimal
        assert_finalizationstate(proposer, {'currentEpoch': 7,
                                            'currentDynasty': 5,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 5,
                                            'validators': 2})


        resp = finalizer1.getvalidatorinfo()
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        # Check that the validator is not included in the next dynasty
        connect_nodes(proposer, finalizer1.index)
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 80)
        sync_blocks([proposer, finalizer1, finalizer2])
        sync_mempools([proposer, finalizer1, finalizer2])

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        assert_finalizationstate(proposer, {'currentEpoch': 8,
                                            'currentDynasty': 6,
                                            'lastJustifiedEpoch': 7,
                                            'lastFinalizedEpoch': 6,
                                            'validators': 1})

        resp = finalizer1.getvalidatorinfo()
        assert_equal(resp["validator_status"], "NOT_VALIDATING")

        # Check that we manage to finalize even with one validator
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 90)
        sync_blocks([proposer, finalizer1, finalizer2])
        sync_mempools([proposer, finalizer1, finalizer2])

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        assert_finalizationstate(proposer, {'currentEpoch': 9,
                                            'currentDynasty': 7,
                                            'lastJustifiedEpoch': 8,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 1})


if __name__ == '__main__':
    EsperanzaLogoutTest().main()
