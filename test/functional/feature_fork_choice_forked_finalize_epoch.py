#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
ForkChoiceForkedFinalizeEpochTest tests that re-org of finalization
is not possible even if the forked epoch is considered finalized
on another fork

Scenario 1: fork after finalized checkpoint
Node shouldn't switch to the fork, even if it has longer justified chain
because epoch=3 is not finalized for that fork
  F        F
[ e3 ] - [ e4 ] - [ e5 ] node
   |
   |                J
   ..] - [ e4 ] - [ e5 ] - [ e6 ] fork

Scenario 2: fork after justified checkpoint
Node shouldn't switch to the fork because its epoch=4 is not finalized
      F        F        F
... [ e2 ] - [ e3 ] - [ e4 ] - [ e5 ] node
                         |
                         |                J
                        .. ] - [ e5 ] - [ e6 ] - [ e6 ] fork

"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    make_vote_tx,
    sync_blocks,
    wait_until,
)
import time


class ForkChoiceForkedFinalizeEpochTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 6
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-deprecatedrpc=accounts', '-proposing=0', esperanza_config],
            ['-deprecatedrpc=accounts', '-proposing=0', esperanza_config],
            ['-deprecatedrpc=accounts', '-proposing=0', esperanza_config, '-validating=1'],

            ['-deprecatedrpc=accounts', '-proposing=0', esperanza_config],
            ['-deprecatedrpc=accounts', '-proposing=0', esperanza_config],
            ['-deprecatedrpc=accounts', '-proposing=0', esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def test_fork_on_finalized_checkpoint(self):
        node = self.nodes[0]
        fork = self.nodes[1]
        finalizer = self.nodes[2]

        self.start_node(node.index)
        self.start_node(fork.index)
        self.start_node(finalizer.index)

        self.setup_stake_coins(node, fork, finalizer)

        connect_nodes(node, fork.index)
        connect_nodes(node, finalizer.index)

        # leave IBD
        self.generate_sync(node, nodes=[node, fork, finalizer])

        # create deposit
        finalizer_address = finalizer.getnewaddress('', 'legacy')
        deposit_tx_id = finalizer.deposit(finalizer_address, 1500)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        generate_block(node)
        disconnect_nodes(node, finalizer.index)

        # leave instant justification
        generate_block(node, count=3 + 5 + 5)
        sync_blocks([node, fork], timeout=10)
        assert_equal(node.getblockcount(), 15)
        assert_finalizationstate(node, {'currentDynasty': 1,
                                        'currentEpoch': 3,
                                        'lastJustifiedEpoch': 2,
                                        'lastFinalizedEpoch': 2,
                                        'validators': 0})

        # create longer justified fork
        # [ e3 ] node
        #    |
        #    |                J
        #    ..] - [ e4 ] - [ e5 ] - [ e6 ] fork
        disconnect_nodes(node, fork.index)
        generate_block(fork, count=5 + 5)
        target = fork.getbestblockhash()
        generate_block(fork)
        vtx = make_vote_tx(finalizer, finalizer_address, target,
                           source_epoch=2, target_epoch=5, input_tx_id=deposit_tx_id)
        fork.sendrawtransaction(vtx)
        generate_block(fork)
        assert_equal(fork.getblockcount(), 27)
        assert_finalizationstate(fork, {'currentDynasty': 2,
                                        'currentEpoch': 6,
                                        'lastJustifiedEpoch': 5,
                                        'lastFinalizedEpoch': 2})

        # create finalization
        #   F
        # [ e3 ] - [ e4 ] node
        #    |
        #    |                J
        #    ..] - [ e4 ] - [ e5 ] - [ e6 ] fork
        generate_block(node)
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        generate_block(node, count=4)
        assert_equal(node.getblockcount(), 20)
        assert_finalizationstate(node, {'currentDynasty': 2,
                                        'currentEpoch': 4,
                                        'lastJustifiedEpoch': 3,
                                        'lastFinalizedEpoch': 3})

        #   F        F
        # [ e3 ] - [ e4 ] - [ e5 ] node
        #    |
        #    |                J
        #    ..] - [ e4 ] - [ e5 ] - [ e6 ] fork
        generate_block(node)
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        generate_block(node, count=4)
        assert_equal(node.getblockcount(), 25)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 4})

        # test that longer justification doesn't trigger re-org before finalization
        connect_nodes(node, fork.index)
        time.sleep(5)  # give enough time to decide

        assert_equal(node.getblockcount(), 25)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 4})

        # TODO: UNIT-E: check that slash transaction was created
        # related issue: #680 #652 #686

        # test that node has valid state after restart
        self.restart_node(node.index)
        assert_equal(node.getblockcount(), 25)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 4})

        # cleanup
        self.stop_node(node.index)
        self.stop_node(fork.index)
        self.stop_node(finalizer.index)

    def test_fork_on_justified_epoch(self):
        node = self.nodes[3]
        fork = self.nodes[4]
        finalizer = self.nodes[5]

        self.start_node(node.index)
        self.start_node(fork.index)
        self.start_node(finalizer.index)

        self.setup_stake_coins(node, fork, finalizer)

        connect_nodes(node, fork.index)
        connect_nodes(node, finalizer.index)

        # leave IBD
        self.generate_sync(node, nodes=[node, fork, finalizer])

        # create deposit
        finalizer_address = finalizer.getnewaddress('', 'legacy')
        finalizer.deposit(finalizer_address, 1500)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        generate_block(node)
        disconnect_nodes(node, finalizer.index)

        # leave instant justification
        #   F        F
        # [ e0 ] - [ e1 ] - [ e2 ] - [ e3 ]
        generate_block(node, count=3 + 5 + 5)
        sync_blocks([node, fork])
        assert_equal(node.getblockcount(), 15)
        assert_finalizationstate(node, {'currentDynasty': 1,
                                        'currentEpoch': 3,
                                        'lastJustifiedEpoch': 2,
                                        'lastFinalizedEpoch': 2,
                                        'validators': 0})

        # justify epoch that will be finalized
        #       F        F
        # ... [ e2 ] - [ e3 ] - [ e4 ] node, fork
        generate_block(node)
        vote = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        vote_tx_id = node.decoderawtransaction(vote)['txid']
        generate_block(node, count=4)
        sync_blocks([node, fork], timeout=10)
        assert_equal(node.getblockcount(), 20)
        assert_finalizationstate(node, {'currentDynasty': 2,
                                        'currentEpoch': 4,
                                        'lastJustifiedEpoch': 3,
                                        'lastFinalizedEpoch': 3})

        # create fork that will be longer justified
        #       F        F
        # ... [ e2 ] - [ e3 ] - [ e4 ] node
        #                          |
        #                          |
        #                         .. ] - [ e5 ] - [ e6 ] fork
        disconnect_nodes(node, fork.index)
        generate_block(fork, count=5 + 5)
        assert_equal(fork.getblockcount(), 30)
        assert_finalizationstate(fork, {'currentDynasty': 3,
                                        'currentEpoch': 6,
                                        'lastJustifiedEpoch': 3,
                                        'lastFinalizedEpoch': 3})

        # create longer justification
        #       F        F
        # ... [ e2 ] - [ e3 ] - [ e4 ] node
        #                          |
        #                          |                J
        #                         .. ] - [ e5 ] - [ e6 ] - [ e7 ] fork
        target = fork.getbestblockhash()
        generate_block(fork)
        vtx = make_vote_tx(finalizer, finalizer_address, target,
                           source_epoch=3, target_epoch=6, input_tx_id=vote_tx_id)
        fork.sendrawtransaction(vtx)
        generate_block(fork, count=4)
        assert_equal(fork.getblockcount(), 35)
        assert_finalizationstate(fork, {'currentDynasty': 3,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 3})

        # finalize epoch=5 on node
        #       F        F        F
        # ... [ e2 ] - [ e3 ] - [ e4 ] - [ e5 ] node
        #                          |
        #                          |                J
        #                         .. ] - [ e5 ] - [ e6 ] - [ e7 ] fork
        generate_block(node)
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        generate_block(node)
        assert_equal(node.getblockcount(), 22)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 4})

        # node shouldn't switch to fork as it's finalization is behind
        connect_nodes(node, fork.index)
        time.sleep(5)
        assert_equal(node.getblockcount(), 22)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 4})

        # TODO: UNIT-E: check that slash transaction was created
        # related issue: #680 #652 #686

        # test that node has valid state after restart
        self.restart_node(node.index)
        assert_equal(node.getblockcount(), 22)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 4})

        # cleanup
        self.stop_node(node.index)
        self.stop_node(fork.index)
        self.stop_node(finalizer.index)

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
