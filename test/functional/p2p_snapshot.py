#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test p2p snapshot messages.

This test checks:
1. Scheme and order of P2P snapshot messages from the syncing node
2. Scheme and order of P2P snapshot messages from the serving node
3. After the fast sync, state of UTXOs of both, syncing and serving node, is the same
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import (
    P2PInterface,
    network_thread_start,
)
from test_framework.blocktools import (
    msg_headers,
    msg_witness_block,
    msg_getsnaphead,
    msg_snaphead,
    msg_getsnapshot,
    msg_snapshot,
    SnapshotHeader,
    GetSnapshot,
    Snapshot,
    UTXO,
    CBlockHeader,
    CBlock,
    COutPoint,
    ser_vector,
    ser_uint256,
    uint256_from_str,
    FromHex,
)
from test_framework.util import (
    assert_equal,
    wait_until,
    bytes_to_hex_str,
    hex_str_to_bytes,
)

import math


class BaseNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.snapshot_requested = False
        self.snapshot_header = SnapshotHeader()
        self.snapshot_data = []
        self.headers = []
        self.parent_block = CBlock()

    def on_getheaders(self, message):
        if len(self.headers) == 0:
            return

        msg = msg_headers()
        for h in self.headers:
            msg.headers.append(h)
        self.send_message(msg)

    def on_getsnaphead(self, message):
        assert(self.snapshot_header.snapshot_hash > 0)
        self.send_message(msg_snaphead(self.snapshot_header))

    def on_snaphead(self, message):
        self.snapshot_header = message.snapshot_header

    def on_getsnapshot(self, message):
        self.snapshot_requested = True

    def on_snapshot(self, message):
        assert_equal(message.snapshot.snapshot_hash, self.snapshot_header.snapshot_hash)
        assert(len(message.snapshot.utxo_subsets) > 0)
        self.snapshot_data += message.snapshot.utxo_subsets

    def on_getdata(self, message):
        for i in message.inv:
            if i.hash == self.parent_block.sha256:
                self.send_message(msg_witness_block(self.parent_block))
                break


class P2PSnapshotTest(UnitETestFramework):
    def __init__(self):
        super().__init__()

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [
            # serving node
            [],

            # syncing node
            ['-prune=1',
             '-isd=1',
             '-snapshotchunktimeout=60',
             '-snapshotdiscoverytimeout=60'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        """
        This test creates the following nodes:
        1. serving_node - full node that has the the snapshot
        2. syncing_p2p - mini node that downloads snapshot from serving_node and tests the protocol
        3. syncing_node - the node which starts with fast sync
        4. serving_p2p - mini node that sends snapshot to syncing_node and tests the protocol
        """
        def has_snapshot(node, height):
            res = node.getblocksnapshot(node.getblockhash(height))
            if 'valid' not in res:
                return False
            return True

        def uint256_from_hex(v):
            return uint256_from_str(hex_str_to_bytes(v))

        def uint256_from_rev_hex(v):
            r = [v[i:i + 2] for i in range(0, len(v), 2)]
            v = ''.join(reversed(r))
            return uint256_from_hex(v)

        serving_node = self.nodes[0]
        syncing_node = self.nodes[1]

        # generate 4 blocks to create the first snapshot
        serving_node.generatetoaddress(4, serving_node.getnewaddress())
        wait_until(lambda: has_snapshot(serving_node, 3))

        syncing_p2p = serving_node.add_p2p_connection(BaseNode())
        serving_p2p = syncing_node.add_p2p_connection(BaseNode())

        # configure serving_p2p to have snapshot header and parent block
        res = serving_node.getblocksnapshot(serving_node.getblockhash(3))
        serving_p2p.snapshot_header = SnapshotHeader(
            snapshot_hash=uint256_from_rev_hex(res['snapshot_hash']),
            block_hash=uint256_from_rev_hex(res['block_hash']),
            stake_modifier=uint256_from_rev_hex(res['stake_modifier']),
            total_utxo_subsets=res['total_utxo_subsets'],
        )
        for i in range(1, 5):
            blockhash = serving_node.getblockhash(i)
            header = CBlockHeader()
            FromHex(header, serving_node.getblockheader(blockhash, False))
            header.calc_sha256()
            serving_p2p.headers.append(header)

            # keep only the parent block
            if serving_p2p.snapshot_header.block_hash == header.hashPrevBlock:
                block = serving_node.getblock(blockhash, False)
                FromHex(serving_p2p.parent_block, block)
                serving_p2p.parent_block.calc_sha256()

        network_thread_start()
        syncing_p2p.wait_for_verack()

        # test snapshot downloading in chunks
        syncing_p2p.send_message(msg_getsnaphead())
        wait_until(lambda: syncing_p2p.snapshot_header.total_utxo_subsets > 0)
        chunks = math.ceil(syncing_p2p.snapshot_header.total_utxo_subsets / 2)
        for i in range(1, chunks+1):
            getsnapshot = GetSnapshot(syncing_p2p.snapshot_header.snapshot_hash, len(syncing_p2p.snapshot_data), 2)
            syncing_p2p.send_message(msg_getsnapshot(getsnapshot))

            snapshot_size = min(i * 2, syncing_p2p.snapshot_header.total_utxo_subsets)
            wait_until(lambda: len(syncing_p2p.snapshot_data) == snapshot_size)
        assert_equal(len(syncing_p2p.snapshot_data), syncing_p2p.snapshot_header.total_utxo_subsets)

        self.log.info('Snapshot was downloaded successfully')

        # validate the snapshot hash
        utxos = []
        for subset in syncing_p2p.snapshot_data:
            for n in subset.outputs:
                out = COutPoint(subset.tx_id, n)
                utxo = UTXO(subset.height, subset.is_coin_base, out, subset.outputs[n])
                utxos.append(utxo)
        inputs = bytes_to_hex_str(ser_vector([]))
        outputs = bytes_to_hex_str(ser_vector(utxos))
        stake_modifier = bytes_to_hex_str(ser_uint256(syncing_p2p.snapshot_header.stake_modifier))
        res = self.nodes[0].calcsnapshothash(inputs, outputs, stake_modifier)
        snapshot_hash = uint256_from_hex(res['hash'])
        assert_equal(snapshot_hash, syncing_p2p.snapshot_header.snapshot_hash)

        self.log.info('Snapshot was validated successfully')

        # test snapshot serving
        wait_until(lambda: serving_p2p.snapshot_requested)
        snapshot = Snapshot(
            snapshot_hash=serving_p2p.snapshot_header.snapshot_hash,
            utxo_subset_index=0,
            utxo_subsets=syncing_p2p.snapshot_data,
        )
        serving_p2p.send_message(msg_snapshot(snapshot))
        wait_until(lambda: syncing_node.getblockcount() == 4)
        assert_equal(serving_node.gettxoutsetinfo(), syncing_node.gettxoutsetinfo())

        self.log.info('Snapshot was sent successfully')


if __name__ == '__main__':
    P2PSnapshotTest().main()
