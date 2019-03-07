#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test node responses to invalid transactions.

In this test we connect to one node over p2p, and test tx requests.
"""

from test_framework.test_framework import ComparisonTestFramework
from test_framework.comptool import TestManager, TestInstance, RejectResult
from test_framework.blocktools import *
from test_framework.util import get_unspent_coins
import time



# Use the ComparisonTestFramework with 1 node: only use --testbinary.
class InvalidTxRequestTest(ComparisonTestFramework):

    ''' Can either run this test as 1 node with expected answers, or two and compare them.
        Change the "outcome" variable from each TestInstance object to only do the comparison. '''
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        test = TestManager(self, self.options.tmpdir)
        test.add_all_connections(self.nodes)
        self.tip = None
        self.block_time = None
        network_thread_start()
        test.run()

    def get_tests(self):
        self.setup_stake_coins(self.nodes[0])

        if self.tip is None:
            self.tip = int("0x" + self.nodes[0].getbestblockhash(), 0)
        self.block_time = int(time.time())+1

        '''
        Create a new block with an anyone-can-spend coinbase
        '''
        height = 1
        snapshot_hash = get_tip_snapshot_meta(self.nodes[0]).hash
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_hash))
        block = create_block(self.tip, coinbase, self.block_time)

        self.block_time += 1
        block.solve()
        # Save the coinbase for later
        self.block1 = block
        self.tip = block.sha256
        height += 1
        yield TestInstance([[block, True]])

        '''
        Now we need that block to mature so we can spend the coinbase.
        '''
        test = TestInstance(sync_every_block=False)
        snapshot_meta = get_tip_snapshot_meta(self.nodes[0])
        for i in range(100):
            prev_coinbase = coinbase
            stake = {'txid': prev_coinbase.hash, 'vout': 0, 'amount': prev_coinbase.vout[0].nValue/UNIT}
            coinbase = create_coinbase(height, stake, snapshot_meta.hash)
            block = create_block(self.tip, coinbase, self.block_time)
            block.solve()
            self.tip = block.sha256
            self.block_time += 1
            test.blocks_and_transactions.append([block, True])

            input_utxo = UTXO(height-1, True, coinbase.vin[1].prevout, prev_coinbase.vout[0])
            output_reward = UTXO(height, True, COutPoint(coinbase.sha256, 0), coinbase.vout[0])
            output_stake = UTXO(height, True, COutPoint(coinbase.sha256, 1), coinbase.vout[1])
            snapshot_meta = calc_snapshot_hash(self.nodes[0], snapshot_meta.data, 0, height, [input_utxo], [output_reward, output_stake])
            height += 1
        yield test

        # b'\x64' is OP_NOTIF
        # Transaction will be rejected with code 16 (REJECT_INVALID)
        tx1 = create_transaction(coinbase, 1, b'\x64' * 35, 50 * UNIT - 12000)
        yield TestInstance([[tx1, RejectResult(16, b'mandatory-script-verify-flag-failed')]], sync_every_tx=True)

        # TODO: test further transactions...

if __name__ == '__main__':
    InvalidTxRequestTest().main()
