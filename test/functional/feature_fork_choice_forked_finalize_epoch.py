#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
ForkChoiceForkedFinalizeEpochTest tests that re-org of finalization
is not possible even if the forked epoch is considered finalized
on another fork

Scenario 1: fork after finalized checkpoint
Node shouldn't switch to the fork, even if it has longer justified channel
because epoch=4 is not finalized for that fork
  F        J
[ e4 ] - [ e5 ] - [ e6 ] node
   |
   |                J
   ..] - [ e5 ] - [ e6 ] - [ e7 ] fork

Scenario 2: fork after justified checkpoint
Node shouldn't switch to the fork because its epoch=4 is not finalized
      F        F        J
... [ e3 ] - [ e4 ] - [ e5 ] - [ e6 ] node
                         |
                         |                J
                        .. ] - [ e6 ] - [ e7 ] - [ e8 ] fork

"""
from test_framework.test_framework import UnitETestFramework
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.util import (
    connect_nodes,
    disconnect_nodes,
    sync_blocks,
    assert_equal,
    wait_until,
)
import time


class ForkChoiceForkedFinalizeEpochTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 8
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],
            ['-proposing=0', esperanza_config, '-validating=1'],

            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],
            ['-proposing=0', esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def test_fork_on_finalized_checkpoint(self):
        node = self.nodes[0]
        fork = self.nodes[1]

        finalizer1 = self.nodes[2]
        finalizer2 = self.nodes[3]

        self.start_node(node.index)
        self.start_node(fork.index)
        self.start_node(finalizer1.index)
        self.start_node(finalizer2.index)

        finalizer1.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        finalizer2.importmasterkey(regtest_mnemonics[0]['mnemonics'])

        connect_nodes(node, fork.index)
        connect_nodes(node, finalizer1.index)
        connect_nodes(node, finalizer2.index)

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress())
        sync_blocks([node, fork, finalizer1])

        # create deposit
        disconnect_nodes(node, finalizer2.index)
        payto = finalizer1.getnewaddress('', 'legacy')
        txid1 = finalizer1.deposit(payto, 10000)
        finalizer2.setaccount(payto, '')
        txid2 = finalizer2.deposit(payto, 10000)
        assert_equal(txid1, txid2)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(1, node.getnewaddress())
        disconnect_nodes(node, finalizer1.index)

        # leave instant justification
        node.generatetoaddress(2 + 5 + 5 + 5 + 5, node.getnewaddress())
        sync_blocks([node, fork])
        assert_equal(node.getblockcount(), 24)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 2)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 4)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 3)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 2)

        # create longer justified fork
        # [ e4 ] node
        #    |
        #    |                J
        #    ..] - [ e5 ] - [ e6 ] - [ e7 ] fork
        disconnect_nodes(node, fork.index)
        fork.generatetoaddress(5 + 5 + 1, fork.getnewaddress())
        connect_nodes(fork, finalizer2.index)
        wait_until(lambda: len(fork.getrawmempool()) > 0, timeout=10)
        fork.generatetoaddress(1, fork.getnewaddress())
        disconnect_nodes(fork, finalizer2.index)
        assert_equal(fork.getblockcount(), 36)
        assert_equal(fork.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(fork.getfinalizationstate()['currentEpoch'], 7)
        assert_equal(fork.getfinalizationstate()['lastJustifiedEpoch'], 6)
        assert_equal(fork.getfinalizationstate()['lastFinalizedEpoch'], 3)

        # create finalization
        #   J
        # [ e4 ] - [ e5 ] node
        #    |
        #    |                J
        #    ..] - [ e5 ] - [ e6 ] - [ e7 ] fork
        connect_nodes(node, finalizer1.index)

        node.generatetoaddress(1, node.getnewaddress())
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(4, node.getnewaddress())
        assert_equal(node.getblockcount(), 29)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 3)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 4)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 3)

        #   F        J
        # [ e4 ] - [ e5 ] - [ e6 ] node
        #    |
        #    |                J
        #    ..] - [ e5 ] - [ e6 ] - [ e7 ] fork
        node.generatetoaddress(1, node.getnewaddress())
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(4, node.getnewaddress())
        assert_equal(node.getblockcount(), 34)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 6)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 4)

        disconnect_nodes(node, finalizer1.index)

        # test that longer justification doesn't trigger re-org before finalization
        connect_nodes(node, fork.index)
        time.sleep(5)  # give enough time to decide

        assert_equal(node.getblockcount(), 34)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 6)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 4)

        # TODO: UNIT-E: check that slash transaction was created
        # related issue: #680 #652 #686

        # test that node has valid state after restart
        self.restart_node(node.index)
        assert_equal(node.getblockcount(), 34)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 6)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 4)

        # cleanup
        self.stop_node(node.index)
        self.stop_node(fork.index)
        self.stop_node(finalizer1.index)
        self.stop_node(finalizer2.index)

    def test_fork_on_justified_epoch(self):
        node = self.nodes[4]
        fork = self.nodes[5]

        finalizer1 = self.nodes[6]
        finalizer2 = self.nodes[7]

        self.start_node(node.index)
        self.start_node(fork.index)
        self.start_node(finalizer1.index)
        self.start_node(finalizer2.index)

        finalizer1.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        finalizer2.importmasterkey(regtest_mnemonics[0]['mnemonics'])

        connect_nodes(node, fork.index)
        connect_nodes(node, finalizer1.index)
        connect_nodes(node, finalizer2.index)

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress())
        sync_blocks([node, fork, finalizer1])

        # create deposit
        disconnect_nodes(node, finalizer2.index)
        payto = finalizer1.getnewaddress('', 'legacy')
        txid1 = finalizer1.deposit(payto, 10000)
        finalizer2.setaccount(payto, '')
        txid2 = finalizer2.deposit(payto, 10000)
        assert_equal(txid1, txid2)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(1, node.getnewaddress())
        disconnect_nodes(node, finalizer1.index)

        # leave instant justification
        #   F        F        F        J
        # [ e0 ] - [ e1 ] - [ e2 ] - [ e3 ] - [ e4 ]
        node.generatetoaddress(2 + 5 + 5 + 5 + 5, node.getnewaddress())
        sync_blocks([node, fork])
        assert_equal(node.getblockcount(), 24)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 2)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 4)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 3)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 2)

        # justify epoch that will be finalized
        #       F        J
        # ... [ e3 ] - [ e4 ] - [ e5 ] node, fork
        connect_nodes(node, finalizer1.index)
        node.generatetoaddress(1, node.getnewaddress())
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(4, node.getnewaddress())
        sync_blocks([node, fork])
        assert_equal(node.getblockcount(), 29)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 3)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 4)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 3)

        # create longer justified fork
        #       F        J
        # ... [ e3 ] - [ e4 ] - [ e5 ] node
        #                          |
        #                          |                J
        #                         .. ] - [ e6 ] - [ e7 ] - [ e8 ] fork
        disconnect_nodes(node, fork.index)
        fork.generatetoaddress(5 + 5, fork.getnewaddress())
        assert_equal(fork.getblockcount(), 39)
        assert_equal(fork.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(fork.getfinalizationstate()['currentEpoch'], 7)
        assert_equal(fork.getfinalizationstate()['lastJustifiedEpoch'], 4)
        assert_equal(fork.getfinalizationstate()['lastFinalizedEpoch'], 3)

        connect_nodes(fork, finalizer2.index)
        fork.generatetoaddress(1, fork.getnewaddress())
        wait_until(lambda: len(fork.getrawmempool()) > 0, timeout=10)
        fork.generatetoaddress(4, fork.getnewaddress())
        assert_equal(fork.getblockcount(), 44)
        assert_equal(fork.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(fork.getfinalizationstate()['currentEpoch'], 8)
        assert_equal(fork.getfinalizationstate()['lastJustifiedEpoch'], 7)
        assert_equal(fork.getfinalizationstate()['lastFinalizedEpoch'], 3)
        disconnect_nodes(fork, finalizer2.index)

        # finalize epoch=4 on node
        #       F        F        J
        # ... [ e3 ] - [ e4 ] - [ e5 ] - [ e6 ] node
        #                          |
        #                          |                J
        #                         .. ] - [ e6 ] - [ e7 ] - [ e8 ] fork
        node.generatetoaddress(1, node.getnewaddress())
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getblockcount(), 31)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 6)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 4)
        disconnect_nodes(node, finalizer1.index)

        # node shouldn't switch to fork as it's finalization is behind
        connect_nodes(node, fork.index)
        time.sleep(5)
        assert_equal(node.getblockcount(), 31)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 6)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 4)

        # TODO: UNIT-E: check that slash transaction was created
        # related issue: #680 #652 #686

        # test that node has valid state after restart
        self.restart_node(node.index)
        assert_equal(node.getblockcount(), 31)
        assert_equal(node.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node.getfinalizationstate()['currentEpoch'], 6)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 4)

        # cleanup
        self.stop_node(node.index)
        self.stop_node(fork.index)
        self.stop_node(finalizer1.index)
        self.stop_node(finalizer2.index)

    def run_test(self):
        self.stop_nodes()

        self.log.info("run test_fork_on_finalized_checkpoint")
        self.test_fork_on_finalized_checkpoint()
        self.log.info("test_fork_on_finalized_checkpoint passed")

        self.log.info("run test_fork_on_justified_epoch")
        self.test_fork_on_justified_epoch()
        self.log.info("test_fork_on_justified_epoch passed")


if __name__ == '__main__':
    ForkChoiceForkedFinalizeEpochTest().main()
