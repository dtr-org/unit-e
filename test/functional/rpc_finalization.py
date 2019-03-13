#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test finalization RPCs:
1. getfinalizationstate
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    wait_until,
    sync_blocks,
)


class RpcFinalizationTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config, '-validating=1'],
            [esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def test_getfinalizationstate(self):
        def create_deposit(finalizer, node):
            connect_nodes(finalizer, node.index)
            payto = finalizer.getnewaddress('', 'legacy')
            txid = finalizer.deposit(payto, 10000)
            wait_until(lambda: txid in node.getrawmempool())
            disconnect_nodes(finalizer, node.index)

        node = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]

        self.setup_stake_coins(node, finalizer1, finalizer2)

        # initial setup
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)

        # leave IBD
        connect_nodes(node, finalizer1.index)
        connect_nodes(node, finalizer2.index)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        sync_blocks([node, finalizer1, finalizer2])
        disconnect_nodes(node, finalizer1.index)
        disconnect_nodes(node, finalizer2.index)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)

        self.log.info('initial finalization state is correct')

        # add finalizer1
        create_deposit(finalizer1, node)

        # test state of last checkpoint
        # e0
        node.generatetoaddress(3, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 4)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)
        self.log.info('finalization state includes new validators')

        # test instant justification 1
        # J
        # e0 - e1
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 9)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 1)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 1 is correct')

        # test instant justification 2
        # F    J
        # e0 - e1 - e2
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 14)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 2)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 1)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 2 is correct')

        # test instant justification 3
        # F    F    J
        # e0 - e1 - e2 - e3
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 19)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 3)
        assert_equal(state['currentDynasty'], 1)
        assert_equal(state['lastFinalizedEpoch'], 1)
        assert_equal(state['lastJustifiedEpoch'], 2)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 3 is correct')

        # test instant justification 4
        # F    F    F    J
        # e0 - e1 - e2 - e3 - e4
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 24)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 4)
        assert_equal(state['currentDynasty'], 2)
        assert_equal(state['lastFinalizedEpoch'], 2)
        assert_equal(state['lastJustifiedEpoch'], 3)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 4 is correct')

        # test instant justification 5 (must be last one)
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 29)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 5)
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)
        self.log.info('instant finalization 5 is correct')

        # no justification
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 34)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 6)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)

        # no justification
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7[35]
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 35)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 7)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state without justification is correct')

        # create first justification
        # F    F    F    F    J         J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7[35, 36]
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))

        assert_equal(node.getblockcount(), 36)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 7)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 6)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state after justification is correct')

        # skip 1 justification
        # F    F    F    J              J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9[45]
        node.generatetoaddress(9, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 45)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 9)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 6)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state without justification is correct')

        # create finalization
        # F    F    F    J              J         J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9[45, 46]
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 46)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 9)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 8)
        assert_equal(state['validators'], 1)

        # F    F    F    J              J         F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10[50, 51]
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 51)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 10)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 8)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['validators'], 1)

        # F    F    F    J              J         F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10
        node.generatetoaddress(3, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 54)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 10)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 8)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['validators'], 1)

        self.log.info('finalization state after finalization is correct')

        # F    F    F    F    J              J    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11[55]
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 55)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 11)
        assert_equal(state['currentDynasty'], 5)
        assert_equal(state['lastFinalizedEpoch'], 8)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['validators'], 1)
        self.log.info('dynasty after finalization is updated correctly')

        # add finalizer2 deposit at dynasty=5. will vote at dynasty=8
        create_deposit(finalizer2, node)

        # F    F    F    F    J              J    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 59)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 11)
        assert_equal(state['currentDynasty'], 5)
        assert_equal(state['lastFinalizedEpoch'], 9)
        assert_equal(state['lastJustifiedEpoch'], 10)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J              J    F    F    F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 64)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 12)
        assert_equal(state['currentDynasty'], 6)
        assert_equal(state['lastFinalizedEpoch'], 10)
        assert_equal(state['lastJustifiedEpoch'], 11)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J              J    F    F    F     F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12 - e13
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 69)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 13)
        assert_equal(state['currentDynasty'], 7)
        assert_equal(state['lastFinalizedEpoch'], 11)
        assert_equal(state['lastJustifiedEpoch'], 12)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J              J    F    F    F     F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12 - e13 - e14[70]
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 70)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 14)
        assert_equal(state['currentDynasty'], 8)
        assert_equal(state['lastFinalizedEpoch'], 11)
        assert_equal(state['lastJustifiedEpoch'], 12)
        assert_equal(state['validators'], 2)
        self.log.info('new deposit was activated correctly')

        # F    F    F    F    J              J    F    F    F     F     F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12 - e13 - e14
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 74)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 14)
        assert_equal(state['currentDynasty'], 8)
        assert_equal(state['lastFinalizedEpoch'], 12)
        assert_equal(state['lastJustifiedEpoch'], 13)
        assert_equal(state['validators'], 2)
        self.log.info('new finalizer votes')

    def run_test(self):
        self.test_getfinalizationstate()
        self.log.info('test_getfinalizationstate passed')


if __name__ == '__main__':
    RpcFinalizationTest().main()
