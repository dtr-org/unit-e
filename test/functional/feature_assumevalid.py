#!/usr/bin/env python3
# Copyright (c) 2014-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test logic for skipping signature validation on old blocks.

Test logic for skipping signature validation on blocks which we've assumed
valid (https://github.com/bitcoin/bitcoin/pull/9484)

We build a chain that includes and invalid signature for one of the
transactions:

    0:        genesis block
    1:        block 1 with coinbase transaction output.
    2-101:    bury that block with 100 blocks so the coinbase transaction
              output can be spent
    102:      a block containing a transaction spending the coinbase
              transaction output. The transaction has an invalid signature.
    103-2202: bury the bad block with just over two weeks' worth of blocks
              (2100 blocks)

Start three nodes:

    - node0 has no -assumevalid parameter. Try to sync to block 2202. It will
      reject block 102 and only sync as far as block 101
    - node1 has -assumevalid set to the hash of block 102. Try to sync to
      block 2202. node1 will sync all the way to block 2202.
    - node2 has -assumevalid set to the hash of block 102. Try to sync to
      block 200. node2 will reject block 102 since it's assumed valid, but it
      isn't buried by at least two weeks' work.
"""
import time

from test_framework.blocktools import (
    calc_snapshot_hash,
    create_block,
    create_coinbase,
    get_tip_snapshot_meta,
    sign_coinbase,
    update_snapshot_with_tx,
)
from test_framework.keytools import KeyTool
from test_framework.messages import (
    COutPoint,
    CBlockHeader,
    CTransaction,
    CTxIn,
    CTxOut,
    TxType,
    UTXO,
    msg_block,
    msg_headers,
)
from test_framework.mininode import (
    P2PInterface,
)
from test_framework.script import (CScript, OP_TRUE)
from test_framework.test_framework import UnitETestFramework, PROPOSER_REWARD
from test_framework.util import assert_equal, connect_nodes_bi
from test_framework.regtest_mnemonics import regtest_mnemonics

class BaseNode(P2PInterface):
    def send_header_for_blocks(self, new_blocks):
        headers_message = msg_headers()
        headers_message.headers = [CBlockHeader(b) for b in new_blocks]
        self.send_message(headers_message)


ESPERANZA_CONFIG = '-esperanzaconfig={"epochLength": 200}'


class AssumeValidTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [
            [ESPERANZA_CONFIG],
            [ESPERANZA_CONFIG],
            [ESPERANZA_CONFIG],
        ]

    def send_blocks_until_disconnected(self, p2p_conn):
        """Keep sending blocks to the node until we're disconnected."""
        for i in range(len(self.blocks)):
            if not p2p_conn.is_connected:
                break
            try:
                p2p_conn.send_message(msg_block(self.blocks[i]))
            except IOError:
                assert not p2p_conn.is_connected
                break

    def assert_blockchain_height(self, node, height):
        """Wait until the blockchain is no longer advancing and verify it's reached the expected height."""
        last_height = node.getblock(node.getbestblockhash())['height']
        timeout = 10
        while True:
            time.sleep(0.25)
            current_height = node.getblock(node.getbestblockhash())['height']
            if current_height != last_height:
                last_height = current_height
                if timeout < 0:
                    assert False, "blockchain too short after timeout: %d" % current_height
                timeout - 0.25
                continue
            elif current_height > height:
                assert False, "blockchain too long: %d" % current_height
            elif current_height == height:
                break

    def build_coins_to_stake(self):
        self.nodes[0].importmasterkey(regtest_mnemonics[0]['mnemonics'])

        address = self.nodes[0].getnewaddress()
        outputs = [{'address': address, 'amount': 1} for x in range(2500)]
        self.nodes[0].sendtypeto('', '', outputs)

        self.nodes[0].importmasterkey(regtest_mnemonics[1]['mnemonics'])
        self.nodes[0].generate(1)[0]

        self.coins_to_stake = [x for x in self.nodes[0].listunspent() if x['amount'] == 1]

    def sync_first_block(self):
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 1, 2)
        self.sync_all()

        # We now stop the other nodes yet since we need to pre-mine a block with
        # an invalid transaction signature so we can pass in the block hash as
        # assumevalid.
        self.stop_node(1)
        self.stop_node(2)

    def get_coin_to_stake(self):
        return self.coins_to_stake.pop()

    def run_test(self):

        # Create a block with 2500 stakeable outputs
        self.build_coins_to_stake()

        # Propagate it to nodes 1 and 2 and stop them for now
        self.sync_first_block()

        # Key Management for node 0
        keytool = KeyTool.for_node(self.nodes[0])

        # Connect to node0
        p2p0 = self.nodes[0].add_p2p_connection(BaseNode())

        # Build the blockchain
        self.tip = int(self.nodes[0].getbestblockhash(), 16)
        self.block_time = self.nodes[0].getblock(self.nodes[0].getbestblockhash())['time'] + 1

        self.blocks = []

        # Get a pubkey for the coinbase TXO
        coinbase_key = keytool.make_privkey()
        coinbase_pubkey = bytes(coinbase_key.get_pubkey())

        keytool.upload_key(coinbase_key)

        self.log.info("Create the first block with a coinbase output to our key")
        height = 2
        snapshot_meta = get_tip_snapshot_meta(self.nodes[0])
        coin = self.get_coin_to_stake()
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_meta.hash, coinbase_pubkey))
        block = create_block(self.tip, coinbase, self.block_time)
        self.blocks.append(block)
        self.block_time += 1
        block.solve()
        # Save the coinbase for later
        self.block1 = block
        self.tip = block.sha256

        utxo1 = UTXO(height, TxType.COINBASE, COutPoint(coinbase.sha256, 0), coinbase.vout[0])
        snapshot_meta = update_snapshot_with_tx(self.nodes[0], snapshot_meta, height, coinbase)
        height += 1

        self.log.info("Bury the block 100 deep so the coinbase output is spendable")
        for i in range(100):
            coin = self.get_coin_to_stake()
            coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_meta.hash, coinbase_pubkey))
            block = create_block(self.tip, coinbase, self.block_time)
            block.solve()
            self.blocks.append(block)
            self.tip = block.sha256
            self.block_time += 1
            snapshot_meta = update_snapshot_with_tx(self.nodes[0], snapshot_meta, height, coinbase)
            height += 1

        self.log.info("Create a transaction spending the coinbase output with an invalid (null) signature")
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.block1.vtx[0].sha256, 0), scriptSig=b""))
        tx.vout.append(CTxOut((PROPOSER_REWARD - 1) * 100000000, CScript([OP_TRUE])))
        tx.calc_sha256()

        coin = self.get_coin_to_stake()
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_meta.hash, coinbase_pubkey))
        block102 = create_block(self.tip, coinbase, self.block_time)
        self.block_time += 1
        block102.vtx.extend([tx])
        block102.compute_merkle_trees()
        block102.rehash()
        block102.solve()
        self.blocks.append(block102)
        self.tip = block102.sha256
        self.block_time += 1

        snapshot_meta = update_snapshot_with_tx(self.nodes[0], snapshot_meta, height, coinbase)

        utxo2 = UTXO(height, tx.get_type(), COutPoint(tx.sha256, 0), tx.vout[0])
        snapshot_meta = calc_snapshot_hash(self.nodes[0], snapshot_meta, height, [utxo1], [utxo2])

        height += 1

        self.log.info("Bury the assumed valid block 2100 deep")
        for i in range(2100):
            coin = self.get_coin_to_stake()
            coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, snapshot_meta.hash, coinbase_pubkey))
            block = create_block(self.tip, coinbase, self.block_time)
            block.nVersion = 4
            block.solve()
            self.blocks.append(block)
            self.tip = block.sha256
            self.block_time += 1
            snapshot_meta = update_snapshot_with_tx(self.nodes[0], snapshot_meta, height, coinbase)
            height += 1

        self.nodes[0].disconnect_p2ps()

        self.log.info("Start node1 and node2 with assumevalid so they accept a block with a bad signature.")
        self.start_node(1, extra_args=["-assumevalid=" + hex(block102.sha256), ESPERANZA_CONFIG])
        self.start_node(2, extra_args=["-assumevalid=" + hex(block102.sha256), ESPERANZA_CONFIG])

        p2p0 = self.nodes[0].add_p2p_connection(BaseNode())
        p2p1 = self.nodes[1].add_p2p_connection(BaseNode())
        p2p2 = self.nodes[2].add_p2p_connection(BaseNode())

        # send header lists to all three nodes
        p2p0.send_header_for_blocks(self.blocks[0:2000])
        p2p0.send_header_for_blocks(self.blocks[2000:])
        p2p1.send_header_for_blocks(self.blocks[0:2000])
        p2p1.send_header_for_blocks(self.blocks[2000:])
        p2p2.send_header_for_blocks(self.blocks[0:200])

        self.log.info("Send blocks to node0. Block 103 will be rejected.")
        self.send_blocks_until_disconnected(p2p0)
        self.assert_blockchain_height(self.nodes[0], 102)

        self.log.info("Send all blocks to node1. All blocks will be accepted.")
        for i in range(2202):
            p2p1.send_message(msg_block(self.blocks[i]))
        # Syncing 2200 blocks can take a while on slow systems. Give it plenty of time to sync.
        p2p1.sync_with_ping(120)
        assert_equal(self.nodes[1].getblock(self.nodes[1].getbestblockhash())['height'], 2203)

        self.log.info("Send blocks to node2. Block 102 will be rejected.")
        self.send_blocks_until_disconnected(p2p2)
        self.assert_blockchain_height(self.nodes[2], 102)

if __name__ == '__main__':
    AssumeValidTest().main()
