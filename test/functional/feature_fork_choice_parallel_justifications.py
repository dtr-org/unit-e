#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test fork choice rule between parallel justification forks

The following checks are performed:
1. node re-orgs to the longer justified parallel fork
2. node re-orgs to the previous fork that became longer justified one
3. node doesn't re-org before finalization
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.messages import (
    msg_block,
)
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    assert_not_equal,
    base58check_to_bytes,
    bytes_to_hex_str,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
    wait_until,
)
from test_framework.messages import (
    CTransaction,
    CBlock,
    FromHex,
)
from test_framework.regtest_mnemonics import regtest_mnemonics

def make_vote_tx(finalizer, finalizer_address, target_hash, source_epoch, target_epoch, input_tx_id):
    finalizer_address = bytes_to_hex_str(base58check_to_bytes(finalizer_address)[::-1])
    vote = {
	'validator_address': finalizer_address,
	'target_hash': target_hash,
	'target_epoch': target_epoch,
	'source_epoch': source_epoch
    }
    vtx = finalizer.createvotetransaction(vote, input_tx_id)
    vtx = finalizer.signrawtransactionwithwallet(vtx)
    return vtx['hex']

class ForkChoiceParallelJustificationsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1', '-finalizervotefromepochblocknumber=9999999'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def sync_node_to_fork(node, fork, force=False):
            if force:
                self.restart_node(node.index, cleanup=True)
                node.importmasterkey(regtest_mnemonics[node.index]['mnemonics'])
            connect_nodes(node, fork.index)
            block_hash = fork.getblockhash(fork.getblockcount())
            node.waitforblock(block_hash, 5000)
            assert_equal(node.getblockhash(node.getblockcount()), block_hash)
            disconnect_nodes(node, fork.index)

        def generate_epoch_and_vote(node, finalizer, finalizer_address, prevtx):
            assert node.getblockcount() % 5 == 0
            fs = node.getfinalizationstate()
            checkpoint = node.getbestblockhash()
            node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
            vtx = make_vote_tx(finalizer, finalizer_address, checkpoint,
                               source_epoch=fs['lastJustifiedEpoch'],
                               target_epoch=fs['currentEpoch'],
                               input_tx_id=prevtx)
            node.sendrawtransaction(vtx)
            checkpoint = node.generatetoaddress(4, fork1.getnewaddress('', 'bech32'))
            vtx = FromHex(CTransaction(), vtx)
            vtx.rehash()
            return vtx.hash

        node = self.nodes[0]
        fork1 = self.nodes[1]
        fork2 = self.nodes[2]
        finalizer = self.nodes[3]

        node.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        fork1.importmasterkey(regtest_mnemonics[1]['mnemonics'])
        fork2.importmasterkey(regtest_mnemonics[2]['mnemonics'])
        finalizer.importmasterkey(regtest_mnemonics[3]['mnemonics'])

        connect_nodes(node, fork1.index)
        connect_nodes(node, fork2.index)
        connect_nodes(node, finalizer.index)

        # leave IBD
        self.generate_sync(node, 1)

        finalizer_address = finalizer.getnewaddress('', 'legacy')
        deptx = finalizer.deposit(finalizer_address, 1500)
        self.wait_for_transaction(deptx)

        # leave insta justification
        #                             -  fork1
        # F    F    F    F    J       |
        # e0 - e1 - e2 - e3 - e4 - e5 -  node
        #                             |
        #                             -  fork2
        node.generatetoaddress(24, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 25)
        sync_blocks([node, finalizer])
        assert_finalizationstate(node, {'currentDynasty': 2,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 0})
        sync_blocks(self.nodes)
        disconnect_nodes(node, fork1.index)
        disconnect_nodes(node, fork2.index)
        disconnect_nodes(node, finalizer.index)

        # create first justified epoch on fork1
        #                                J
        #                             - e6 - e7 - e8 fork1 node
        # F    F    F    F    J       |
        # e0 - e1 - e2 - e3 - e4 - e5 -
        #                             |
        #                             -  fork2

        fork1.generatetoaddress(5, fork1.getnewaddress('', 'bech32'))
        vtx1 = generate_epoch_and_vote(fork1, finalizer, finalizer_address, deptx)
        fork1.generatetoaddress(5, fork1.getnewaddress('', 'bech32'))
        assert_equal(fork1.getblockcount(), 40)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 8,
                                         'lastJustifiedEpoch': 6,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        sync_node_to_fork(node, fork1)

        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 8,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 1})

        self.log.info('node successfully switched to the justified fork')

        # create longer justified epoch on fork2
        # node must switch ("zig") to this fork
        #                                J
        #                             - e6 - e7 - e8 fork1
        # F    F    F    F    J       |
        # e0 - e1 - e2 - e3 - e4 - e5 -
        #                             |       J
        #                             - e6 - e7 - e8 fork2 node

        fork2.generatetoaddress(10, fork2.getnewaddress('', 'bech32'))
        vtx2 = generate_epoch_and_vote(fork2, finalizer, finalizer_address, deptx)
        assert_equal(fork2.getblockcount(), 40)
        assert_finalizationstate(fork2, {'currentDynasty': 3,
                                         'currentEpoch': 8,
                                         'lastJustifiedEpoch': 7,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        sync_node_to_fork(node, fork2)

        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 8,
                                        'lastJustifiedEpoch': 7,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 1})

        self.log.info('node successfully switched to the longest justified fork')

        # create longer justified epoch on the previous fork1
        # node must switch ("zag") to this fork
        #                                J              J
        #                             - e6 - e7 - e8 - e9 - e10 fork1 node
        # F    F    F    F    J       |
        # e0 - e1 - e2 - e3 - e4 - e5 -
        #                             |       J
        #                             - e6 - e7 - e8 fork2
        fork1.generatetoaddress(5, fork1.getnewaddress('', 'bech32'))
        sync_node_to_fork(finalizer, fork1)
        vtx1 = generate_epoch_and_vote(fork1, finalizer, finalizer_address, vtx1)
        assert_equal(fork1.getblockcount(), 50)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 10,
                                         'lastJustifiedEpoch': 9,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        assert_not_equal(fork1.getbestblockhash(), fork2.getbestblockhash())
        sync_node_to_fork(node, fork1)
        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 10,
                                        'lastJustifiedEpoch': 9,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 1})

        self.log.info('node successfully switched back to the longest justified fork')

        # UNIT-E TODO: node must follow longest finalized chain
        # node follows longest finalization
        #                                J              J
        #                             - e6 - e7 - e8 - e9 - e10 fork1 node
        # F    F    F    F    J       |
        # e0 - e1 - e2 - e3 - e4 - e5 -
        #                             |       F    J
        #                             - e6 - e7 - e8 - e9 fork2

        sync_node_to_fork(finalizer, fork2, force=True)
        vtx2 = generate_epoch_and_vote(fork2, finalizer, finalizer_address, vtx2)
        assert_equal(fork2.getblockcount(), 45)
        assert_finalizationstate(fork2, {'currentDynasty': 3,
                                         'currentEpoch': 9,
                                         'lastJustifiedEpoch': 8,
                                         'lastFinalizedEpoch': 7,
                                         'validators': 1})

        # UNIT-E TODO: fix here
        # sync_node_to_fork(node, fork2)
        # assert_finalizationstate(node, {'currentDynasty': 3,
        #                                 'currentEpoch': 9,
        #                                 'lastJustifiedEpoch': 8,
        #                                 'lastFinalizedEpoch': 7,
        #                                 'validators': 1})
        # self.log.info('node successfully switched to the longest finalized fork')

if __name__ == '__main__':
    ForkChoiceParallelJustificationsTest().main()
