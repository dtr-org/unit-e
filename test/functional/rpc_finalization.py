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
            txid = finalizer.deposit(payto, 1500)
            wait_until(lambda: txid in node.getrawmempool())
            disconnect_nodes(finalizer, node.index)

        node = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]

        self.setup_stake_coins(node, finalizer1, finalizer2)

        # initial setup
        # F
        # e0
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['validators'], 0)

        # start epoch=1
        # F
        # e0 - e1[1]
        connect_nodes(node, finalizer1.index)
        connect_nodes(node, finalizer2.index)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        sync_blocks([node, finalizer1, finalizer2])
        disconnect_nodes(node, finalizer1.index)
        disconnect_nodes(node, finalizer2.index)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['currentEpoch'], 1)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['validators'], 0)

        self.log.info('initial finalization state is correct')

        # add finalizer1
        create_deposit(finalizer1, node)

        # test instant justification 1
        # F
        # e0 - e1
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 5)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['currentEpoch'], 1)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['validators'], 0)
        self.log.info('finalization state includes new validators')

        # test instant justification 2
        # F    J
        # e0 - e1 - e2
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 10)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['currentEpoch'], 2)
        assert_equal(state['lastJustifiedEpoch'], 1)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 1 is correct')

        # test instant justification 3
        # F    F    J
        # e0 - e1 - e2 - e3
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 15)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['currentEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 2)
        assert_equal(state['lastFinalizedEpoch'], 1)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 2 is correct')

        # test instant justification 4
        # F    F    F    J
        # e0 - e1 - e2 - e3 - e4
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 20)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 1)
        assert_equal(state['currentEpoch'], 4)
        assert_equal(state['lastJustifiedEpoch'], 3)
        assert_equal(state['lastFinalizedEpoch'], 2)
        assert_equal(state['validators'], 0)
        self.log.info('instant finalization 3 is correct')

        # test instant justification 5 (must be last one)
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 25)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 2)
        assert_equal(state['currentEpoch'], 5)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 0)

        # no justification
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 26)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 6)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 1)

        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 30)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 6)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 1)

        # no justification
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7[31]
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 31)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 7)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state without justification is correct')

        # create first justification
        # F    F    F    F    J         J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7[31, 32]
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))

        assert_equal(node.getblockcount(), 32)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 7)
        assert_equal(state['lastJustifiedEpoch'], 6)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state after justification is correct')

        # skip 1 justification
        # F    F    F    F    J         J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9[41]
        node.generatetoaddress(9, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 41)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 9)
        assert_equal(state['lastJustifiedEpoch'], 6)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state without justification is correct')

        # create finalization
        # F    F    F    J              J         J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9[41, 42]
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 42)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 9)
        assert_equal(state['lastJustifiedEpoch'], 8)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J         J         F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10[46, 47]
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 47)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 10)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['lastFinalizedEpoch'], 8)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J         J         F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10
        node.generatetoaddress(3, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 50)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['currentEpoch'], 10)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['lastFinalizedEpoch'], 8)
        assert_equal(state['validators'], 1)

        self.log.info('finalization state after finalization is correct')

        # F    F    F    F    J              J    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11[51]
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 51)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['currentEpoch'], 11)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['lastFinalizedEpoch'], 8)
        assert_equal(state['validators'], 1)
        self.log.info('dynasty after finalization is updated correctly')

        # add finalizer2 deposit at dynasty=5. will vote at dynasty=8
        create_deposit(finalizer2, node)

        # F    F    F    F    J              J    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 55)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['currentEpoch'], 11)
        assert_equal(state['lastJustifiedEpoch'], 10)
        assert_equal(state['lastFinalizedEpoch'], 9)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J              J    F    F    F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 60)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 5)
        assert_equal(state['currentEpoch'], 12)
        assert_equal(state['lastJustifiedEpoch'], 11)
        assert_equal(state['lastFinalizedEpoch'], 10)
        assert_equal(state['validators'], 1)

        # F    F    F    J                   J    F    F    F     F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12 - e13
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 65)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 6)
        assert_equal(state['currentEpoch'], 13)
        assert_equal(state['lastJustifiedEpoch'], 12)
        assert_equal(state['lastFinalizedEpoch'], 11)
        assert_equal(state['validators'], 1)

        # F    F    F    F    J              J    F    F    F     F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12 - e13 - e14[66]
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 66)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 7)
        assert_equal(state['currentEpoch'], 14)
        assert_equal(state['lastJustifiedEpoch'], 12)
        assert_equal(state['lastFinalizedEpoch'], 11)
        assert_equal(state['validators'], 2)
        self.log.info('new deposit was activated correctly')

        # F    F    F    F    J              J    F    F    F     F     F     J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6 - e7 - e8 - e9 - e10 - e11 - e12 - e13 - e14
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 70)
        state = node.getfinalizationstate()
        assert_equal(state['currentDynasty'], 7)
        assert_equal(state['currentEpoch'], 14)
        assert_equal(state['lastJustifiedEpoch'], 13)
        assert_equal(state['lastFinalizedEpoch'], 12)
        assert_equal(state['validators'], 2)
        self.log.info('new finalizer votes')

    def run_test(self):
        self.test_getfinalizationstate()
        self.log.info('test_getfinalizationstate passed')


if __name__ == '__main__':
    RpcFinalizationTest().main()
