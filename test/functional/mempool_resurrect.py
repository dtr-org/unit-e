#!/usr/bin/env python3
# Copyright (c) 2014-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test resurrection of mined transactions when the blockchain is re-organized."""

from decimal import Decimal

from test_framework.blocktools import create_raw_transaction
from test_framework.test_framework import UnitETestFramework, PROPOSER_REWARD
from test_framework.util import assert_equal, assert_finalizationstate


class MempoolCoinbaseTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):

        node = self.nodes[0]
        self.setup_stake_coins(node)

        first_3_blocks = node.generate(3)

        # Let's lock the first 3 coinbase txs so we can used them later
        for block_id in first_3_blocks:
            node.lockunspent(False, [{"txid": node.getblock(block_id)['tx'][0], "vout": 0}])

        # Make the first 3 coinbase mature now
        node.generate(102)
        assert_equal(node.getblockcount(), 105)
        assert_finalizationstate(node, {'currentDynasty': 19,
                                        'currentEpoch': 21,
                                        'lastJustifiedEpoch': 20,
                                        'lastFinalizedEpoch': 20})

        node0_address = node.getnewaddress("", "bech32")
        # Spend block 1/2/3's coinbase transactions
        # Mine a block.
        # Create three more transactions, spending the spends
        # Mine another block.
        # ... make sure all the transactions are confirmed
        # Invalidate both blocks
        # ... make sure all the transactions are put back in the mempool
        # Mine a new block
        # ... make sure all the transactions are confirmed again.

        b = [self.nodes[0].getblockhash(n) for n in range(1, 4)]
        coinbase_txids = [self.nodes[0].getblock(h)['tx'][0] for h in b]
        spends1_raw = [create_raw_transaction(self.nodes[0], txid, node0_address, amount=PROPOSER_REWARD - Decimal('0.01')) for txid in coinbase_txids]
        spends1_id = [self.nodes[0].sendrawtransaction(tx) for tx in spends1_raw]

        blocks = []
        blocks.extend(node.generate(1))

        spends2_raw = [create_raw_transaction(self.nodes[0], txid, node0_address, amount=PROPOSER_REWARD - Decimal('0.02')) for txid in spends1_id]
        spends2_id = [self.nodes[0].sendrawtransaction(tx) for tx in spends2_raw]

        blocks.extend(node.generate(1))

        # mempool should be empty, all txns confirmed
        assert_equal(set(node.getrawmempool()), set())
        for txid in spends1_id+spends2_id:
            tx = node.gettransaction(txid)
            assert tx["confirmations"] > 0

        # Use invalidateblock to re-org back
        for node in self.nodes:
            node.invalidateblock(blocks[0])

        # All txns should be back in mempool with 0 confirmations
        assert_equal(set(node.getrawmempool()), set(spends1_id+spends2_id))
        for txid in spends1_id+spends2_id:
            tx = node.gettransaction(txid)
            assert tx["confirmations"] == 0

        # Generate another block, they should all get mined
        node.generate(1)
        # mempool should be empty, all txns confirmed
        assert_equal(set(node.getrawmempool()), set())
        for txid in spends1_id+spends2_id:
            tx = node.gettransaction(txid)
            assert tx["confirmations"] > 0


if __name__ == '__main__':
    MempoolCoinbaseTest().main()
