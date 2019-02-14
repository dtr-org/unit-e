#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test finalization RPCs:
1. getfinalizationstate
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.admin import Admin
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    wait_until,
    sync_blocks,
)

class RpcFinalizationTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def test_getfinalizationstate(self):
        node = self.nodes[0]
        finalizer = self.nodes[1]

        node.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        finalizer.importmasterkey(regtest_mnemonics[1]['mnemonics'])

        connect_nodes(node, finalizer.index)

        # initial setup
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress())
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)

        self.log.info('initial finalization state is correct')

        # add finalizer
        Admin.authorize_and_disable(self, node)
        payto = finalizer.getnewaddress('', 'legacy')
        txid = finalizer.deposit(payto, 10000)
        self.wait_for_transaction(txid, timeout=150)
        disconnect_nodes(node, finalizer.index)

        # test state of last checkpoint
        node.generatetoaddress(2, node.getnewaddress())
        assert_equal(node.getblockcount(), 4)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state includes new validators')

        # test instant finalization 1
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getblockcount(), 5)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 1)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 1)
        self.log.info('instant finalization 1 is correct')

        # test instant finalization 2
        node.generatetoaddress(4, node.getnewaddress())
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getblockcount(), 10)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 2)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 1)
        assert_equal(state['validators'], 1)
        self.log.info('instant finalization 2 is correct')

        # test instant finalization 3
        node.generatetoaddress(5, node.getnewaddress())
        assert_equal(node.getblockcount(), 15)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 3)
        assert_equal(state['currentDynasty'], 1)
        assert_equal(state['lastFinalizedEpoch'], 1)
        assert_equal(state['lastJustifiedEpoch'], 2)
        assert_equal(state['validators'], 1)
        self.log.info('instant finalization 3 is correct')

        # test instant finalization 4
        node.generatetoaddress(5, node.getnewaddress())
        assert_equal(node.getblockcount(), 20)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 4)
        assert_equal(state['currentDynasty'], 2)
        assert_equal(state['lastFinalizedEpoch'], 2)
        assert_equal(state['lastJustifiedEpoch'], 3)
        assert_equal(state['validators'], 1)
        self.log.info('instant finalization 4 is correct')

        # test instant finalization 5 (must be last one)
        node.generatetoaddress(5, node.getnewaddress())
        assert_equal(node.getblockcount(), 25)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 5)
        assert_equal(state['currentDynasty'], 3)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)
        self.log.info('instant finalization 5 is correct')

        # no justification
        node.generatetoaddress(5, node.getnewaddress())
        assert_equal(node.getblockcount(), 30)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 6)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)

        # no justification
        node.generatetoaddress(5, node.getnewaddress())
        assert_equal(node.getblockcount(), 35)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 7)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)

        # no justification
        node.generatetoaddress(5, node.getnewaddress())
        assert_equal(node.getblockcount(), 40)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 8)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 4)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state without justification is correct')

        # create first justification
        connect_nodes(node, finalizer.index)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=15)
        node.generatetoaddress(1, node.getnewaddress())
        disconnect_nodes(node, finalizer.index)

        assert_equal(node.getblockcount(), 41)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 8)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 7)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state after justification is correct')

        # skip 1 justification
        node.generatetoaddress(9, node.getnewaddress())
        assert_equal(node.getblockcount(), 50)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 10)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 7)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state without justification is correct')

        # create finalization
        connect_nodes(node, finalizer.index)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=15)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getblockcount(), 51)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 10)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 3)
        assert_equal(state['lastJustifiedEpoch'], 9)
        assert_equal(state['validators'], 1)

        node.generatetoaddress(4, node.getnewaddress())
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=15)
        node.generatetoaddress(1, node.getnewaddress())
        disconnect_nodes(node, finalizer.index)
        assert_equal(node.getblockcount(), 56)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 11)
        assert_equal(state['currentDynasty'], 4)
        assert_equal(state['lastFinalizedEpoch'], 9)
        assert_equal(state['lastJustifiedEpoch'], 10)
        assert_equal(state['validators'], 1)
        self.log.info('finalization state after finalization is correct')

        node.generatetoaddress(4, node.getnewaddress())
        assert_equal(node.getblockcount(), 60)
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 12)
        assert_equal(state['currentDynasty'], 5)
        assert_equal(state['lastFinalizedEpoch'], 9)
        assert_equal(state['lastJustifiedEpoch'], 10)
        assert_equal(state['validators'], 1)
        self.log.info('dynasty after finalization is updated correctly')

    def run_test(self):
        self.test_getfinalizationstate()
        self.log.info('test_getfinalizationstate passed')


if __name__ == '__main__':
    RpcFinalizationTest().main()
