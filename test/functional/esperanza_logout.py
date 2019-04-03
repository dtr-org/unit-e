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
    assert_raises_rpc_error,
    wait_until,
)
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
        sync_blocks([finalizer1, finalizer2], timeout=10)

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

        # Generate enough blocks to advance 3 dynasties and have active finalizers
        proposer.generate(5 * 10)
        assert_equal(proposer.getblockcount(), 51)
        assert_finalizationstate(proposer, {'currentEpoch': 6,
                                            'currentDynasty': 3,
                                            'lastJustifiedEpoch': 4,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 2})

        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)

        # Mine the votes to avoid the next logout to conflict with a vote in the mempool
        proposer.generate(1)
        assert_equal(proposer.getblockcount(), 52)

        # Logout included in dynasty=3
        # At dynasty=3+3=6 finalizer is still voting
        # At dynasty=7 finalizer doesn't vote
        connect_nodes(finalizer1, proposer.index)
        sync_blocks([finalizer1, proposer], timeout=10)
        logout_tx = finalizer1.logout()
        wait_until(lambda: logout_tx in proposer.getrawmempool(), timeout=10)
        disconnect_nodes(finalizer1, proposer.index)

        # Check that the finalizer is still voting for epoch 6
        proposer.generate(9)
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)

        # Check that we cannot logout again
        assert_raises_rpc_error(-8, 'Cannot send logout transaction.', finalizer1.logout)

        # Mine votes and move to checkpoint
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 70)
        assert_finalizationstate(proposer, {'currentEpoch': 7,
                                            'currentDynasty': 4,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 5,
                                            'validators': 2})

        # Check that the finalizer is still voting up to dynasty=6 (including)
        for _ in range(2):
            proposer.generate(1)
            self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
            self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
            proposer.generate(9)

        assert_equal(proposer.getblockcount(), 90)
        assert_finalizationstate(proposer, {'currentEpoch': 9,
                                            'currentDynasty': 6,
                                            'lastJustifiedEpoch': 8,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 2})

        # finalizer1 is logged out
        proposer.generate(1)
        assert_equal(proposer.getblockcount(), 91)
        assert_finalizationstate(proposer, {'currentEpoch': 10,
                                            'currentDynasty': 7,
                                            'lastJustifiedEpoch': 8,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 1})

        # finalizer1 is not validating so we can keep it connected
        connect_nodes(finalizer1, proposer.index)
        sync_blocks([finalizer1, proposer], timeout=10)
        wait_until(lambda: finalizer1.getvalidatorinfo()['validator_status'] == 'NOT_VALIDATING', timeout=5)
        assert_raises_rpc_error(-8, 'The node is not validating.', finalizer1.logout)

        # Check that we manage to finalize even with one finalizer
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
        proposer.generate(9)
        assert_equal(proposer.getblockcount(), 100)
        assert_finalizationstate(proposer, {'currentEpoch': 10,
                                            'currentDynasty': 7,
                                            'lastJustifiedEpoch': 9,
                                            'lastFinalizedEpoch': 8,
                                            'validators': 1})


if __name__ == '__main__':
    EsperanzaLogoutTest().main()
