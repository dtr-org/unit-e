#!/usr/bin/env python3
# Copyright (c) 2015-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test node responses to invalid transactions.

In this test we connect to one node over p2p, and test tx requests."""

from decimal import Decimal

from test_framework.blocktools import (
    calc_snapshot_hash,
    create_block,
    create_coinbase,
    create_tx_with_script,
    get_tip_snapshot_meta,
    sign_coinbase,
)
from test_framework.messages import (
    UNIT,
    UTXO,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    TxType,
)
from test_framework.mininode import P2PDataStore
from test_framework.test_framework import UnitETestFramework, PROPOSER_REWARD
from test_framework.util import (
    assert_equal,
    get_unspent_coins,
    wait_until,
)
from data import invalid_txs


class InvalidTxRequestTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def bootstrap_p2p(self, *, num_connections=1):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        for _ in range(num_connections):
            self.nodes[0].add_p2p_connection(P2PDataStore())

    def reconnect_p2p(self, **kwargs):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p(**kwargs)

    def run_test(self):
        node = self.nodes[0]  # convenience reference to the node

        self.setup_stake_coins(self.nodes[0])

        self.bootstrap_p2p()  # Add one p2p connection to the node

        best_block = self.nodes[0].getbestblockhash()
        tip = int(best_block, 16)
        best_block_time = self.nodes[0].getblock(best_block)['time']
        block_time = best_block_time + 1

        self.log.info("Create a new block with an anyone-can-spend coinbase.")
        height = 1
        snapshot_hash = get_tip_snapshot_meta(self.nodes[0]).hash
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_hash))
        block = create_block(tip, coinbase, block_time)

        block_time += 1
        block.solve()
        # Save the coinbase for later
        block1 = block
        tip = block.sha256
        node.p2p.send_blocks_and_test([block], node, success=True)

        self.log.info("Mature the block.")

        blocks = []
        snapshot_meta = get_tip_snapshot_meta(self.nodes[0])
        for i in range(100):
            prev_coinbase = coinbase
            height += 1
            stake = {'txid': prev_coinbase.hash, 'vout': 1, 'amount': prev_coinbase.vout[1].nValue/UNIT}
            coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, stake, snapshot_meta.hash))
            block = create_block(tip, coinbase, block_time)
            block.solve()
            tip = block.sha256
            block_time += 1
            blocks.append(block)

            input_utxo = UTXO(height-1, TxType.COINBASE, coinbase.vin[1].prevout, prev_coinbase.vout[1])
            output_reward = UTXO(height, TxType.COINBASE, COutPoint(coinbase.sha256, 0), coinbase.vout[0])
            output_stake = UTXO(height, TxType.COINBASE, COutPoint(coinbase.sha256, 1), coinbase.vout[1])
            snapshot_meta = calc_snapshot_hash(self.nodes[0], snapshot_meta, height, [input_utxo], [output_reward, output_stake], coinbase)
        node.p2p.send_blocks_and_test(blocks, node, success=True)

        # b'\x64' is OP_NOTIF
        # Transaction will be rejected with code 16 (REJECT_INVALID)
        # and we get disconnected immediately
        self.log.info('Test a transaction that is rejected')
        tx1 = create_tx_with_script(coinbase, 1, script_sig=b'\x64' * 35, amount=50 * UNIT - 12000)
        node.p2p.send_txs_and_test([tx1], node, success=False, expect_disconnect=True)

        # UNIT-E TODO [0.18.0]: Should we make use of this snippet?
        # Iterate through a list of known invalid transaction types, ensuring each is
        # rejected. Some are consensus invalid and some just violate policy.
        for BadTxTemplate in invalid_txs.iter_all_templates():
            self.log.info("Testing invalid transaction: %s", BadTxTemplate.__name__)
            template = BadTxTemplate(spend_block=block1)
            tx = template.get_tx()
            node.p2p.send_txs_and_test(
                [tx], node, success=False,
                expect_disconnect=template.expect_disconnect,
                reject_reason=template.reject_reason,
            )

            if template.expect_disconnect:
                self.log.info("Reconnecting to peer")
                self.reconnect_p2p()

        # Make two p2p connections to provide the node with orphans
        # * p2ps[0] will send valid orphan txs (one with low fee)
        # * p2ps[1] will send an invalid orphan tx (and is later disconnected for that)
        self.reconnect_p2p(num_connections=2)

        self.log.info('Test orphan transaction handling ... ')
        # Create a root transaction that we withhold until all dependent transactions
        # are sent out and in the orphan cache
        SCRIPT_PUB_KEY_OP_TRUE = b'\x51\x75' * 15 + b'\x51'
        tx_withhold = CTransaction()
        tx_withhold.vin.append(CTxIn(outpoint=COutPoint(block1.vtx[0].sha256, 0)))
        tx_withhold.vout.append(CTxOut(nValue=PROPOSER_REWARD * UNIT - 12000, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        tx_withhold.calc_sha256()

        # Our first orphan tx with some outputs to create further orphan txs
        tx_orphan_1 = CTransaction()
        tx_orphan_1.vin.append(CTxIn(outpoint=COutPoint(tx_withhold.sha256, 0)))
        tx_orphan_1.vout = [CTxOut(nValue=1 * UNIT, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE)] * 3
        tx_orphan_1.calc_sha256()

        # A valid transaction with low fee
        tx_orphan_2_no_fee = CTransaction()
        tx_orphan_2_no_fee.vin.append(CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 0)))
        tx_orphan_2_no_fee.vout.append(CTxOut(nValue=1 * UNIT, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))

        # A valid transaction with sufficient fee
        tx_orphan_2_valid = CTransaction()
        tx_orphan_2_valid.vin.append(CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 1)))
        tx_orphan_2_valid.vout.append(CTxOut(nValue=1 * UNIT - 12000, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))
        tx_orphan_2_valid.calc_sha256()

        # An invalid transaction with negative fee
        tx_orphan_2_invalid = CTransaction()
        tx_orphan_2_invalid.vin.append(CTxIn(outpoint=COutPoint(tx_orphan_1.sha256, 2)))
        tx_orphan_2_invalid.vout.append(CTxOut(nValue=Decimal('1.1') * UNIT, scriptPubKey=SCRIPT_PUB_KEY_OP_TRUE))

        self.log.info('Send the orphans ... ')
        # Send valid orphan txs from p2ps[0]
        node.p2p.send_txs_and_test([tx_orphan_1, tx_orphan_2_no_fee, tx_orphan_2_valid], node, success=False)
        # Send invalid tx from p2ps[1]
        node.p2ps[1].send_txs_and_test([tx_orphan_2_invalid], node, success=False)

        assert_equal(0, node.getmempoolinfo()['size'])  # Mempool should be empty
        assert_equal(2, len(node.getpeerinfo()))  # p2ps[1] is still connected

        self.log.info('Send the withhold tx ... ')
        with node.assert_debug_log(expected_msgs=["bad-txns-in-belowout"]):
            node.p2p.send_txs_and_test([tx_withhold], node, success=True)

        # Transactions that should end up in the mempool
        expected_mempool = {
            t.hash
            for t in [
                tx_withhold,  # The transaction that is the root for all orphans
                tx_orphan_1,  # The orphan transaction that splits the coins
                tx_orphan_2_valid,  # The valid transaction (with sufficient fee)
            ]
        }
        # Transactions that do not end up in the mempool
        # tx_orphan_no_fee, because it has too low fee (p2ps[0] is not disconnected for relaying that tx)
        # tx_orphan_invaid, because it has negative fee (p2ps[1] is disconnected for relaying that tx)

        wait_until(lambda: 1 == len(node.getpeerinfo()), timeout=12)  # p2ps[1] is no longer connected
        assert_equal(expected_mempool, set(node.getrawmempool()))



if __name__ == '__main__':
    InvalidTxRequestTest().main()
