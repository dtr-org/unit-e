#!/usr/bin/env python3
# Copyright (c) 2016-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test NULLDUMMY softfork.

Connect to a single node.
[Policy/Consensus] Check that NULLDUMMY compliant transactions are accepted.
[Policy/Consensus] Check that the new NULLDUMMY rules are enforced
"""
import time

from test_framework.blocktools import (
    create_block,
    create_coinbase,
    create_transaction,
    get_tip_snapshot_meta,
    sign_coinbase,
)
from test_framework.messages import CTransaction, msg_block
from test_framework.mininode import P2PInterface
from test_framework.script import CScript
from test_framework.test_framework import UnitETestFramework, PROPOSER_REWARD
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    get_unspent_coins,
)

NULLDUMMY_ERROR = "non-mandatory-script-verify-flag (Dummy CHECKMULTISIG argument must be zero) (code 64)"

def trueDummy(tx):
    scriptSig = CScript(tx.vin[0].scriptSig)
    newscript = []
    for i in scriptSig:
        if (len(newscript) == 0):
            assert len(i) == 0
            newscript.append(b'\x51')
        else:
            newscript.append(i)
    tx.vin[0].scriptSig = CScript(newscript)
    tx.rehash()

class NULLDUMMYTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # This script tests NULLDUMMY, which is part of 'segwit',
        self.extra_args = [['-whitelist=127.0.0.1', '-addresstype=legacy', "-deprecatedrpc=addwitnessaddress"]]

    def run_test(self):
        self.setup_stake_coins(self.nodes[0])

        self.address = self.nodes[0].getnewaddress()
        self.ms_address = self.nodes[0].addmultisigaddress(1, [self.address])['address']
        self.wit_address = self.nodes[0].getnewaddress(address_type='p2sh-segwit')
        self.wit_ms_address = self.nodes[0].addmultisigaddress(1, [self.address], '', 'p2sh-segwit')['address']

        p2p = self.nodes[0].add_p2p_connection(P2PInterface())

        self.coinbase_blocks = self.nodes[0].generate(2)  # Block 2
        coinbase_txid = []
        for i in self.coinbase_blocks:
            coinbase_txid.append(self.nodes[0].getblock(i)['tx'][0])
        self.nodes[0].generate(427)  # Block 429
        self.lastblockhash = self.nodes[0].getbestblockhash()
        self.tip = int("0x" + self.lastblockhash, 0)
        self.lastblockheight = self.nodes[0].getblockcount()
        self.lastblocktime = int(time.time()) + 2

        ms_tx = create_transaction(self.nodes[0], coinbase_txid[0], self.ms_address, amount=PROPOSER_REWARD - 1)
        ms_txid = self.nodes[0].sendrawtransaction(bytes_to_hex_str(ms_tx.serialize_with_witness()), True)

        wit_ms_tx = create_transaction(self.nodes[0], coinbase_txid[1], self.wit_ms_address, amount=PROPOSER_REWARD - 1)
        wit_ms_txid = self.nodes[0].sendrawtransaction(bytes_to_hex_str(wit_ms_tx.serialize_with_witness()), True)

        self.send_block(self.nodes[0], [ms_tx, wit_ms_tx], True)

        self.log.info("Test 1: Non-NULLDUMMY base multisig transaction is invalid")
        test1tx = create_transaction(self.nodes[0], ms_txid, self.address, amount=PROPOSER_REWARD - 2)
        test3txs=[CTransaction(test1tx)]
        trueDummy(test1tx)
        assert_raises_rpc_error(-26, NULLDUMMY_ERROR, self.nodes[0].sendrawtransaction, bytes_to_hex_str(test1tx.serialize_with_witness()), True)
        self.send_block(self.nodes[0], [test1tx])

        self.log.info("Test 2: Non-NULLDUMMY P2WSH multisig transaction invalid")
        test2tx = create_transaction(self.nodes[0], wit_ms_txid, self.wit_address, amount=PROPOSER_REWARD - 2)
        test3txs.append(CTransaction(test2tx))
        test2tx.wit.vtxinwit[0].scriptWitness.stack[0] = b'\x01'
        assert_raises_rpc_error(-26, NULLDUMMY_ERROR, self.nodes[0].sendrawtransaction, bytes_to_hex_str(test2tx.serialize_with_witness()), True)
        self.send_block(self.nodes[0], [test2tx])

        self.log.info("Test 3: NULLDUMMY compliant base/witness transactions should be accepted to mempool")
        for i in test3txs:
            self.nodes[0].sendrawtransaction(bytes_to_hex_str(i.serialize_with_witness()), True)
        self.send_block(self.nodes[0], test3txs, True)


    def send_block(self, node, txs, accept = False):
        snapshot_hash = get_tip_snapshot_meta(self.nodes[0]).hash
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        block = create_block(self.tip, create_coinbase(self.lastblockheight + 1, coin, snapshot_hash), self.lastblocktime + 1)
        block.vtx[0] = sign_coinbase(self.nodes[0], block.vtx[0])
        block.nVersion = 4
        for tx in txs:
            tx.rehash()
            block.vtx.append(tx)

        block.ensure_ltor()
        block.compute_merkle_trees()
        block.solve()

        node.p2p.send_and_ping(msg_block(block))

        if (accept):
            assert_equal(node.getbestblockhash(), block.hash)
            self.tip = block.sha256
            self.lastblockhash = block.hash
            self.lastblocktime += 1
            self.lastblockheight += 1
        else:
            assert_equal(node.getbestblockhash(), self.lastblockhash)

if __name__ == '__main__':
    NULLDUMMYTest().main()
