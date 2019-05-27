#!/usr/bin/env python3
# Copyright (c) 2015-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test BIP65 (CHECKLOCKTIMEVERIFY).

Test that the CHECKLOCKTIMEVERIFY soft-fork activates at (regtest) block height
1351.
"""

from test_framework.blocktools import create_coinbase, create_block, create_transaction, get_tip_snapshot_meta, sign_coinbase
from test_framework.messages import CTransaction, msg_block, ToHex
from test_framework.mininode import P2PInterface
from test_framework.script import CScript, OP_1NEGATE, OP_CHECKLOCKTIMEVERIFY, OP_DROP, CScriptNum
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    bytes_to_hex_str,
   get_unspent_coins, hex_str_to_bytes,
)

from io import BytesIO

# Reject codes that we might receive in this test
REJECT_INVALID = 16
REJECT_NONSTANDARD = 64

def cltv_invalidate(tx):
    '''Modify the signature in vin 0 of the tx to fail CLTV

    Prepends -1 CLTV DROP in the scriptSig itself.

    TODO: test more ways that transactions using CLTV could be invalid (eg
    locktime requirements fail, sequence time requirements fail, etc).
    '''
    tx.vin[0].scriptSig = CScript([OP_1NEGATE, OP_CHECKLOCKTIMEVERIFY, OP_DROP] +
                                  list(CScript(tx.vin[0].scriptSig)))

def cltv_validate(node, tx, height):
    '''Modify the signature in vin 0 of the tx to pass CLTV
    Prepends <height> CLTV DROP in the scriptSig, and sets
    the locktime to height'''
    tx.vin[0].nSequence = 0
    tx.nLockTime = height

    # Need to re-sign, since nSequence and nLockTime changed
    signed_result = node.signrawtransactionwithwallet(ToHex(tx))
    new_tx = CTransaction()
    new_tx.deserialize(BytesIO(hex_str_to_bytes(signed_result['hex'])))

    new_tx.vin[0].scriptSig = CScript([CScriptNum(height), OP_CHECKLOCKTIMEVERIFY, OP_DROP] +
                                  list(CScript(new_tx.vin[0].scriptSig)))
    return new_tx


class BIP65Test(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-whitelist=127.0.0.1', '-par=1']]  # Use only one script thread to get the exact reject reason for testing
        self.setup_clean_chain = True
        self.rpc_timeout = 120

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        self.setup_stake_coins(self.nodes[0])
        self.nodes[0].add_p2p_connection(P2PInterface())

        self.log.info("Mining one block")
        self.coinbase_txids = [self.nodes[0].getblock(b)['tx'][0] for b in self.nodes[0].generate(1)]
        self.nodeaddress = self.nodes[0].getnewaddress()

        self.log.info("Test that invalid-according-to-cltv transactions cannot appear in a block")

        spendtx = create_transaction(self.nodes[0], self.coinbase_txids[0],
                self.nodeaddress, amount=1.0)
        cltv_invalidate(spendtx)
        spendtx.rehash()

        # First we show that this tx is valid except for CLTV by getting it
        # rejected from the mempool for exactly that reason.
        assert_equal(
            [{'txid': spendtx.hash, 'allowed': False, 'reject-reason': '64: non-mandatory-script-verify-flag (Negative locktime)'}],
            self.nodes[0].testmempoolaccept(rawtxs=[bytes_to_hex_str(spendtx.serialize())], allowhighfees=True)
        )

        tip = self.nodes[0].getbestblockhash()
        block_time = self.nodes[0].getblockheader(tip)['mediantime'] + 1
        snapshot_hash = get_tip_snapshot_meta(self.nodes[0]).hash
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(1, coin, snapshot_hash))
        block = create_block(int(tip, 16), coinbase, block_time)
        block.nVersion = 4
        block.vtx.append(spendtx)
        block.compute_merkle_trees()
        block.solve()

        with self.nodes[0].assert_debug_log(expected_msgs=['CheckInputs on {} failed with non-mandatory-script-verify-flag (Negative locktime)'.format(block.vtx[-1].hash)]):
            self.nodes[0].p2p.send_and_ping(msg_block(block))
            assert_equal(self.nodes[0].getbestblockhash(), tip)
            self.nodes[0].p2p.sync_with_ping()

        self.log.info("Test that a version 4 block with a valid-according-to-CLTV transaction is accepted")
        spendtx = cltv_validate(self.nodes[0], spendtx, 0)
        spendtx.rehash()

        block.vtx.pop(1)
        block.vtx.append(spendtx)
        block.compute_merkle_trees()
        block.solve()

        self.nodes[0].p2p.send_and_ping(msg_block(block))
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)


if __name__ == '__main__':
    BIP65Test().main()
