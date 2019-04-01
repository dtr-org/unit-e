#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import (
    assert_equal,
    json,
    connect_nodes,
    assert_finalizationstate,
    sync_blocks,
    disconnect_nodes,
    sync_mempools,
    assert_raises_rpc_error)
from test_framework.test_framework import UnitETestFramework


class EsperanzaLogoutTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 3

        params_data = {
            'epochLength': 10,
            'dynastyLogoutDelay': 3
        }
        json_params = json.dumps(params_data)

        finalizer_node_params = [
            '-validating=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-esperanzaconfig=' + json_params]

        self.extra_args = [proposer_node_params,
                           finalizer_node_params,
                           finalizer_node_params]
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

        assert_finalizationstate(proposer, {'currentEpoch': 1,
                                            'currentDynasty': 0,
                                            'lastJustifiedEpoch': 0,
                                            'lastFinalizedEpoch': 0,
                                            'validators': 0})

        disconnect_nodes(finalizer1, proposer.index)
        disconnect_nodes(finalizer2, proposer.index)

        # Generate enough blocks to advance 2 dynasties and have active finalizers
        proposer.generate(40)  # 4 * 10
        assert_equal(proposer.getblockcount(), 41)
        assert_finalizationstate(proposer, {'currentEpoch': 5,
                                            'currentDynasty': 3,
                                            'lastJustifiedEpoch': 4,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 2})

        connect_nodes(finalizer1, proposer.index)
        connect_nodes(finalizer2, proposer.index)

        sync_blocks([proposer, finalizer1, finalizer2])
        sync_mempools([proposer, finalizer1, finalizer2])

        # Mine the votes to avoid the next logout to conflict with a vote in the mempool
        proposer.generate(1)
        assert_equal(proposer.getblockcount(), 42)
        sync_blocks([proposer, finalizer1, finalizer2])

        # Logout included in epoch 5, logout will be effective in dynasty 3+3 = 6
        logout_tx = finalizer1.logout()
        self.wait_for_transaction(logout_tx, 60)

        disconnect_nodes(finalizer1, proposer.index)

        # Check that the finalizer is still voting for epoch 6
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 51)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)

        # Check that we cannot logout again
        assert_raises_rpc_error(-8, "Cannot send logout transaction.", finalizer1.logout)

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        assert_finalizationstate(proposer, {'currentEpoch': 6,
                                            'currentDynasty': 4,
                                            'lastJustifiedEpoch': 5,
                                            'lastFinalizedEpoch': 4,
                                            'validators': 2})

        resp = finalizer1.getvalidatorinfo()
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        # Check that the finalizer is still voting for epoch 7
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 61)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        assert_finalizationstate(proposer, {'currentEpoch': 7,
                                            'currentDynasty': 5,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 5,
                                            'validators': 2})

        resp = finalizer1.getvalidatorinfo()
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        # Check that the finalizer is not included in the next dynasty
        connect_nodes(proposer, finalizer1.index)
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 71)
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

        # Check that we manage to finalize even with one finalizer
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 81)
        sync_blocks([proposer, finalizer1, finalizer2])
        sync_mempools([proposer, finalizer1, finalizer2])

        # Mine votes
        proposer.generate(1)
        sync_blocks([proposer, finalizer2])
        sync_mempools([proposer, finalizer2])

        assert_raises_rpc_error(-8, "The node is not validating.", finalizer1.logout)

        assert_finalizationstate(proposer, {'currentEpoch': 9,
                                            'currentDynasty': 7,
                                            'lastJustifiedEpoch': 8,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 1})


if __name__ == '__main__':
    EsperanzaLogoutTest().main()
