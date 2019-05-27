#!/usr/bin/env python3
# Copyright (c) 2015-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test node responses to invalid blocks.

In this test we connect to one node over p2p, and test block requests:
1) Valid blocks should be requested and become chain tip.
2) Invalid block with duplicated transaction should be re-requested.
3) Invalid block with bad coinbase value should be rejected and not
re-requested.
"""
import copy

from test_framework.blocktools import (
    calc_snapshot_hash,
    create_block,
    create_coinbase,
    create_tx_with_script,
    get_tip_snapshot_meta,
    sign_coinbase,
)
from test_framework.messages import UNIT, UTXO, COutPoint, TxType
from test_framework.mininode import P2PDataStore
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal, get_unspent_coins

class InvalidBlockRequestTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-whitelist=127.0.0.1"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        # Add p2p connection to node0
        node = self.nodes[0]  # convenience reference to the node
        node.add_p2p_connection(P2PDataStore())

        self.setup_stake_coins(node)

        best_block = node.getblock(node.getbestblockhash())
        tip = int(node.getbestblockhash(), 16)
        height = best_block["height"] + 1
        block_time = best_block["time"] + 1

        self.log.info("Create a new block with an anyone-can-spend coinbase")

        height = 1
        snapshot_hash = get_tip_snapshot_meta(self.nodes[0]).hash
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_hash, n_pieces=10))
        block = create_block(tip, coinbase, block_time)
        block_time += 1
        block.ensure_ltor()
        block.compute_merkle_trees()
        block.solve()
        # Save the coinbase for later
        block1 = block
        tip = block.sha256
        node.p2p.send_blocks_and_test([block1], node, success=True)
        # UNIT-E TODO [0.18.0]: Deleted in 0.18
        # height += 1

        self.log.info("Mature the block.")

        snapshot_meta = get_tip_snapshot_meta(self.nodes[0])
        blocks = []
        for i in range(100):
            prev_coinbase = coinbase
            stake = {'txid': prev_coinbase.hash, 'vout': 1, 'amount': prev_coinbase.vout[1].nValue/UNIT}
            coinbase = sign_coinbase(node, create_coinbase(height, stake, snapshot_meta.hash))
            block = create_block(tip, coinbase, block_time)
            block.solve()
            tip = block.sha256
            block_time += 1
            blocks.append(block)

            input_utxo = UTXO(height-1, TxType.COINBASE, coinbase.vin[1].prevout, prev_coinbase.vout[1])
            output_reward = UTXO(height, TxType.COINBASE, COutPoint(coinbase.sha256, 0), coinbase.vout[0])
            output_stake = UTXO(height, TxType.COINBASE, COutPoint(coinbase.sha256, 1), coinbase.vout[1])
            snapshot_meta = calc_snapshot_hash(self.nodes[0], snapshot_meta, height, [input_utxo], [output_reward, output_stake], coinbase)

            height += 1
        node.p2p.send_blocks_and_test(blocks, node, success=True)

        '''
        Now we use merkle-root malleability to generate an invalid block with
        same blockheader.
        Manufacture a block with 3 transactions (coinbase, spend of prior
        coinbase, spend of that spend).  Duplicate the 3rd transaction to
        leave merkle root and blockheader unchanged but invalidate the block.
        '''
        prev_coinbase = coinbase
        stake = {'txid': prev_coinbase.hash, 'vout': 1, 'amount': prev_coinbase.vout[1].nValue/UNIT}
        snapshot_meta = get_tip_snapshot_meta(self.nodes[0])
        coinbase = sign_coinbase(node, create_coinbase(height, stake, snapshot_meta.hash))
        block2 = create_block(tip, coinbase, block_time)
        block_time += 1

        # b'0x51' is OP_TRUE
        tx1 = create_tx_with_script(block1.vtx[0], 0, script_sig=b'\x51', amount=50 * UNIT)
        tx2 = create_tx_with_script(tx1, 0, script_sig=b'\x51', amount=50 * UNIT)

        block2.vtx.extend([tx1, tx2])
        block2.ensure_ltor()
        block2.compute_merkle_trees()
        block2.solve()
        orig_hash = block2.sha256
        block2_orig = copy.deepcopy(block2)

        # Mutate block 2
        block2.vtx.append(block2.vtx[-1])
        assert_equal(block2.hashMerkleRoot, block2.calc_merkle_root())
        assert_equal(orig_hash, block2.rehash())
        assert block2_orig.vtx != block2.vtx

        node.p2p.send_blocks_and_test([block2], node, success=False, reject_reason='bad-txns-duplicate')

        # Check transactions for duplicate inputs
        self.log.info("Test duplicate input block.")

        block2_orig.vtx[2].vin.append(block2_orig.vtx[2].vin[0])
        block2_orig.vtx[2].rehash()
        block2_orig.ensure_ltor()
        block2_orig.compute_merkle_trees()
        block2_orig.solve()
        node.p2p.send_blocks_and_test([block2_orig], node, success=False, reject_reason='bad-txns-inputs-duplicate')

        self.log.info("Test very broken block.")

        stake = {'txid': prev_coinbase.hash, 'vout': 1, 'amount': prev_coinbase.vout[1].nValue/UNIT}
        snapshot_meta = get_tip_snapshot_meta(self.nodes[0])
        block3 = create_block(tip, sign_coinbase(node, create_coinbase(height, stake, snapshot_meta.hash)), block_time)
        block_time += 1
        block3.vtx[0].vout[0].nValue = 100 * UNIT  # Too high!
        block3.vtx[0].sha256 = None
        block3.vtx[0].calc_sha256()
        block3.compute_merkle_trees()
        block3.solve()

        node.p2p.send_blocks_and_test([block3], node, success=False, reject_reason='bad-cb-amount')


if __name__ == '__main__':
    InvalidBlockRequestTest().main()
