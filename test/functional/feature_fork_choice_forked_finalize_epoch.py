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
because epoch=5 is not finalized for that fork
  F        J
[ e5 ] - [ e6 ] - [ e7 ] node
   |
   |                J
   ..] - [ e6 ] - [ e7 ] - [ e8 ] fork

Scenario 2: fork after justified checkpoint
Node shouldn't switch to the fork because its epoch=5 is not finalized
      F        F        J
... [ e4 ] - [ e5 ] - [ e6 ] - [ e7 ] node
                         |
                         |                J
                        .. ] - [ e7 ] - [ e8 ] - [ e9 ] fork

"""
from test_framework.test_framework import UnitETestFramework
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.util import (
    connect_nodes,
    assert_finalizationstate,
    bytes_to_hex_str,
    disconnect_nodes,
    sync_blocks,
    assert_equal,
    wait_until,
)
from test_framework.messages import FromHex, ToHex, CTransaction
from test_framework.keytools import BinaryData
import time


def make_vote_tx(finalizer, finalizer_address, target_hash, source_epoch, target_epoch, input_tx_id):
    vote = {
        'validator_address': finalizer_address,
        'target_hash': target_hash,
        'target_epoch': target_epoch,
        'source_epoch': source_epoch
    }
    vtx = finalizer.createvotetransaction(vote, input_tx_id)
    vtx = finalizer.signrawtransaction(vtx)
    vtx = FromHex(CTransaction(), vtx['hex'])
    return vtx


class ForkChoiceForkedFinalizeEpochTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 6
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],

            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],
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

        finalizer.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        node.importmasterkey(regtest_mnemonics[1]['mnemonics'])
        fork.importmasterkey(regtest_mnemonics[2]['mnemonics'])

        connect_nodes(node, fork.index)
        connect_nodes(node, finalizer.index)

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork, finalizer])

        # create deposit
        payto = finalizer.getnewaddress('', 'legacy')
        finalizer_address = bytes_to_hex_str(BinaryData.from_base58check(payto).to_bytes()[::-1])
        deposit_tx_id = finalizer.deposit(payto, 1500)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        disconnect_nodes(node, finalizer.index)

        # leave instant justification
        node.generatetoaddress(3 + 5 + 5 + 5 + 5, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork], timeout=10)
        assert_equal(node.getblockcount(), 25)
        assert_finalizationstate(node, {'currentDynasty': 2,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 0})

        # create longer justified fork
        # [ e5 ] node
        #    |
        #    |                J
        #    ..] - [ e6 ] - [ e7 ] - [ e8 ] fork
        disconnect_nodes(node, fork.index)
        fork.generatetoaddress(5 + 5, fork.getnewaddress('', 'bech32'))
        target = fork.getbestblockhash()
        fork.generatetoaddress(1, fork.getnewaddress('', 'bech32'))
        vtx = make_vote_tx(finalizer, finalizer_address, target,
                           source_epoch=4, target_epoch=7, input_tx_id=deposit_tx_id)
        fork.sendrawtransaction(ToHex(vtx))
        fork.generatetoaddress(1, fork.getnewaddress('', 'bech32'))
        assert_equal(fork.getblockcount(), 37)
        assert_finalizationstate(fork, {'currentDynasty': 3,
                                        'currentEpoch': 8,
                                        'lastJustifiedEpoch': 7,
                                        'lastFinalizedEpoch': 3})

        # create finalization
        #   J
        # [ e5 ] - [ e6 ] node
        #    |
        #    |                J
        #    ..] - [ e6 ] - [ e7 ] - [ e8 ] fork
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 30)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 6,
                                        'lastJustifiedEpoch': 5,
                                        'lastFinalizedEpoch': 4})

        #   F        J
        # [ e5 ] - [ e6 ] - [ e7 ] node
        #    |
        #    |                J
        #    ..] - [ e6 ] - [ e7 ] - [ e8 ] fork
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 35)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 5})

        # test that longer justification doesn't trigger re-org before finalization
        connect_nodes(node, fork.index)
        time.sleep(5)  # give enough time to decide

        assert_equal(node.getblockcount(), 35)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 5})

        # TODO: UNIT-E: check that slash transaction was created
        # related issue: #680 #652 #686

        # test that node has valid state after restart
        self.restart_node(node.index)
        assert_equal(node.getblockcount(), 35)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 5})

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

        finalizer.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        node.importmasterkey(regtest_mnemonics[1]['mnemonics'])
        fork.importmasterkey(regtest_mnemonics[2]['mnemonics'])

        connect_nodes(node, fork.index)
        connect_nodes(node, finalizer.index)

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork, finalizer])

        # create deposit
        payto = finalizer.getnewaddress('', 'legacy')
        finalizer_address = bytes_to_hex_str(BinaryData.from_base58check(payto).to_bytes()[::-1])
        finalizer.deposit(payto, 1500)
        wait_until(lambda: len(node.getrawmempool()) > 0, timeout=10)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        disconnect_nodes(node, finalizer.index)

        # leave instant justification
        #   F        F        F        F        J
        # [ e0 ] - [ e1 ] - [ e2 ] - [ e3 ] - [ e4 ] - [ e5 ]
        node.generatetoaddress(3 + 5 + 5 + 5 + 5, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork])
        assert_equal(node.getblockcount(), 25)
        assert_finalizationstate(node, {'currentDynasty': 2,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 0})

        # justify epoch that will be finalized
        #       F        J
        # ... [ e4 ] - [ e5 ] - [ e6 ] node, fork
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        vote = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        vote_tx_id = node.decoderawtransaction(vote)['txid']
        node.generatetoaddress(4, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork], timeout=10)
        assert_equal(node.getblockcount(), 30)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 6,
                                        'lastJustifiedEpoch': 5,
                                        'lastFinalizedEpoch': 4})

        # create fork that will be longer justified
        #       F        J
        # ... [ e4 ] - [ e5 ] - [ e6 ] node
        #                          |
        #                          |
        #                         .. ] - [ e7 ] - [ e8 ] fork
        disconnect_nodes(node, fork.index)
        fork.generatetoaddress(5 + 5, fork.getnewaddress('', 'bech32'))
        assert_equal(fork.getblockcount(), 40)
        assert_finalizationstate(fork, {'currentDynasty': 4,
                                        'currentEpoch': 8,
                                        'lastJustifiedEpoch': 5,
                                        'lastFinalizedEpoch': 4})

        # create longer justification
        #       F        J
        # ... [ e4 ] - [ e5 ] - [ e6 ] node
        #                          |
        #                          |                J
        #                         .. ] - [ e7 ] - [ e8 ] - [ e9 ] fork
        target = fork.getbestblockhash()
        fork.generatetoaddress(1, fork.getnewaddress('', 'bech32'))
        vtx = make_vote_tx(finalizer, finalizer_address, target,
                           source_epoch=5, target_epoch=8, input_tx_id=vote_tx_id)
        fork.sendrawtransaction(ToHex(vtx))
        fork.generatetoaddress(4, fork.getnewaddress('', 'bech32'))
        assert_equal(fork.getblockcount(), 45)
        assert_finalizationstate(fork, {'currentDynasty': 4,
                                        'currentEpoch': 9,
                                        'lastJustifiedEpoch': 8,
                                        'lastFinalizedEpoch': 4})

        # finalize epoch=5 on node
        #       F        F        J
        # ... [ e4 ] - [ e5 ] - [ e6 ] - [ e7 ] node
        #                          |
        #                          |                J
        #                         .. ] - [ e7 ] - [ e8 ] - [ e9 ] fork
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=node)
        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 32)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 5})

        # node shouldn't switch to fork as it's finalization is behind
        connect_nodes(node, fork.index)
        time.sleep(5)
        assert_equal(node.getblockcount(), 32)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 5})

        # TODO: UNIT-E: check that slash transaction was created
        # related issue: #680 #652 #686

        # test that node has valid state after restart
        self.restart_node(node.index)
        assert_equal(node.getblockcount(), 32)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 7,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 5})

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
