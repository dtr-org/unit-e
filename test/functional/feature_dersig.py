#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test BIP66 (DER SIG).

Test that the DERSIG soft-fork activates at (regtest) height 1251.
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import *
from test_framework.mininode import *
from test_framework.blocktools import create_coinbase, sign_coinbase, create_block, get_tip_snapshot_meta
from test_framework.script import CScript
from io import BytesIO

# Reject codes that we might receive in this test
REJECT_INVALID = 16
REJECT_OBSOLETE = 17
REJECT_NONSTANDARD = 64

# A canonical signature consists of:
# <30> <total len> <02> <len R> <R> <02> <len S> <S> <hashtype>
def unDERify(tx):
    """
    Make the signature in vin 0 of a tx non-DER-compliant,
    by adding padding after the S-value.
    """
    scriptSig = CScript(tx.vin[0].scriptSig)
    newscript = []
    for i in scriptSig:
        if (len(newscript) == 0):
            newscript.append(i[0:-1] + b'\0' + i[-1:])
        else:
            newscript.append(i)
    tx.vin[0].scriptSig = CScript(newscript)

def create_transaction(node, coinbase, to_address, amount):
    from_txid = node.getblock(coinbase)['tx'][0]
    inputs = [{ "txid" : from_txid, "vout" : 0}]
    outputs = { to_address : amount }
    rawtx = node.createrawtransaction(inputs, outputs)
    signresult = node.signrawtransaction(rawtx)
    tx = CTransaction()
    tx.deserialize(BytesIO(hex_str_to_bytes(signresult['hex'])))
    return tx

class BIP66Test(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-promiscuousmempoolflags=1', '-whitelist=127.0.0.1']]
        self.setup_clean_chain = True

    def run_test(self):
        self.setup_stake_coins(self.nodes[0])
        self.nodes[0].add_p2p_connection(P2PInterface())

        network_thread_start()

        # wait_for_verack ensures that the P2P connection is fully up.
        self.nodes[0].p2p.wait_for_verack()

        self.log.info("Mining one block")
        self.coinbase_blocks = self.nodes[0].generate(1)
        self.nodeaddress = self.nodes[0].getnewaddress()

        self.log.info("Test that transactions with non-DER signatures cannot appear in a block")

        spendtx = create_transaction(self.nodes[0], self.coinbase_blocks[0],
                self.nodeaddress, 1.0)
        unDERify(spendtx)
        spendtx.rehash()

        # First we show that this tx is valid except for DERSIG by getting it
        # accepted to the mempool (which we can achieve with
        # -promiscuousmempoolflags).
        self.nodes[0].p2p.send_and_ping(msg_tx(spendtx))
        assert spendtx.hash in self.nodes[0].getrawmempool()

        # Now we verify that a block with this transaction is invalid.
        tip = self.nodes[0].getbestblockhash()
        block_time = self.nodes[0].getblockheader(tip)['mediantime'] + 1
        snapshot_hash = get_tip_snapshot_meta(self.nodes[0]).hash
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(1, coin, snapshot_hash))
        block = create_block(int(tip, 16), coinbase, block_time)
        block.nVersion = 3
        block.vtx.append(spendtx)
        block.hashMerkleRoot = block.calc_merkle_root()
        block.hash_witness_merkle_root = block.calc_witness_merkle_root()
        block.solve()

        self.nodes[0].p2p.send_and_ping(msg_block(block))
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), int(tip, 16))

        wait_until(lambda: "reject" in self.nodes[0].p2p.last_message.keys(), lock=mininode_lock)
        with mininode_lock:
            # We can receive different reject messages depending on whether
            # united is running with multiple script check threads. If script
            # check threads are not in use, then transaction script validation
            # happens sequentially, and united produces more specific reject
            # reasons.
            assert self.nodes[0].p2p.last_message["reject"].code in [REJECT_INVALID, REJECT_NONSTANDARD]
            assert_equal(self.nodes[0].p2p.last_message["reject"].data, block.sha256)
            if self.nodes[0].p2p.last_message["reject"].code == REJECT_INVALID:
                # Generic rejection when a block is invalid
                reject_reason = self.nodes[0].p2p.last_message["reject"].reason
                assert_equal(reject_reason, b'block-validation-failed')
            else:
                assert b'Non-canonical DER signature' in self.nodes[0].p2p.last_message["reject"].reason

        self.log.info("Test that a version 3 block with a DERSIG-compliant transaction is accepted")
        block.vtx[1] = create_transaction(self.nodes[0],
                self.coinbase_blocks[0], self.nodeaddress, 1.0)
        block.hashMerkleRoot = block.calc_merkle_root()
        block.hash_witness_merkle_root = block.calc_witness_merkle_root()
        block.solve()

        self.nodes[0].p2p.send_and_ping(msg_block(block))
        assert_equal(int(self.nodes[0].getbestblockhash(), 16), block.sha256)

if __name__ == '__main__':
    BIP66Test().main()
