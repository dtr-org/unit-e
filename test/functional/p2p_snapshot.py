#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test p2p snapshot messages.

test_p2p_schema checks:
1. Scheme and order of P2P snapshot messages from the syncing node
2. Scheme and order of P2P snapshot messages from the serving node
3. After the fast sync, state of UTXOs of both, syncing and serving node, is the same

test_sync_with_restart checks:
1. the node can start after it discovered snapshot and was stopped
2. the node can start after it downloaded half the snapshot and was stopped
3. the node can start after it downloaded the full snapshot and was stopped
4. the node can start after it downloaded the parent block of the snapshot and was stopped
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import (
    P2PInterface,
    network_thread_start,
    network_thread_join,
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
import os


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

    def update_snapshot_header_from(self, node):
        res = node.listsnapshots()[0]
        self.snapshot_header = SnapshotHeader(
            snapshot_hash=uint256_from_rev_hex(res['snapshot_hash']),
            block_hash=uint256_from_rev_hex(res['block_hash']),
            stake_modifier=uint256_from_rev_hex(res['stake_modifier']),
            total_utxo_subsets=res['total_utxo_subsets'],
        )

    def update_headers_and_blocks_from(self, node):
        self.headers = []

        for i in range(1, node.getblockcount() + 1):
            blockhash = node.getblockhash(i)
            header = CBlockHeader()
            FromHex(header, node.getblockheader(blockhash, False))
            header.calc_sha256()
            self.headers.append(header)

            # keep only the parent block
            if self.snapshot_header.block_hash == header.hashPrevBlock:
                block = node.getblock(blockhash, False)
                FromHex(self.parent_block, block)
                self.parent_block.calc_sha256()


class WaitNode(BaseNode):
    def __init__(self):
        super().__init__()

        self.snapshot_chunk1_requested = False
        self.return_snapshot_chunk1 = False

        self.snapshot_chunk2_requested = False
        self.return_snapshot_chunk2 = False

        self.parent_block_requested = False
        self.return_parent_block = False

    def on_getsnapshot(self, message):
        start = message.getsnapshot.utxo_subset_index
        stop = start + len(self.snapshot_data) / 2
        snapshot = Snapshot(
            snapshot_hash=self.snapshot_header.snapshot_hash,
            utxo_subset_index=message.getsnapshot.utxo_subset_index,
            utxo_subsets=self.snapshot_data[int(start):int(stop)],
        )

        if start == 0:
            self.snapshot_chunk1_requested = True
            if self.return_snapshot_chunk1:
                self.send_message(msg_snapshot(snapshot))
            return

        self.snapshot_chunk2_requested = True
        if self.return_snapshot_chunk2:
            self.send_message(msg_snapshot(snapshot))
        return

    def on_getdata(self, message):
        for i in message.inv:
            if i.hash == self.parent_block.sha256:
                self.parent_block_requested = True
                if self.return_parent_block:
                    self.send_message(msg_witness_block(self.parent_block))
                break


def uint256_from_hex(v):
    return uint256_from_str(hex_str_to_bytes(v))


def uint256_from_rev_hex(v):
    r = [v[i:i + 2] for i in range(0, len(v), 2)]
    v = ''.join(reversed(r))
    return uint256_from_hex(v)


def has_snapshot(node, height):
    res = node.getblocksnapshot(node.getblockhash(height))
    if 'valid' not in res:
        return False
    return True


def assert_chainstate_equal(node1, node2):
    info1 = node1.gettxoutsetinfo()
    info2 = node2.gettxoutsetinfo()
    for k in info1:
        if k == 'disk_size':  # disk size is estimated and may not match
            continue
        assert_equal(info1[k], info2[k])


class P2PSnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [
            # test_p2p_schema
            # serving node
            [],
            # syncing node
            ['-prune=1',
             '-isd=1',
             '-snapshotchunktimeout=60',
             '-snapshotdiscoverytimeout=60'],

            # test_sync_with_restarts
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

    def test_p2p_schema(self):
        """
        This test creates the following nodes:
        1. serving_node - full node that has the the snapshot
        2. syncing_p2p - mini node that downloads snapshot from serving_node and tests the protocol
        3. syncing_node - the node which starts with fast sync
        4. serving_p2p - mini node that sends snapshot to syncing_node and tests the protocol
        """
        serving_node = self.nodes[0]
        syncing_node = self.nodes[1]

        self.start_node(serving_node.index)
        self.start_node(syncing_node.index)

        # generate 4 blocks to create the first snapshot
        serving_node.generatetoaddress(4, serving_node.getnewaddress())
        wait_until(lambda: has_snapshot(serving_node, 3))

        syncing_p2p = serving_node.add_p2p_connection(BaseNode())
        serving_p2p = syncing_node.add_p2p_connection(BaseNode())

        # configure serving_p2p to have snapshot header and parent block
        serving_p2p.update_snapshot_header_from(serving_node)
        serving_p2p.update_headers_and_blocks_from(serving_node)

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

        # clean up test
        serving_node.disconnect_p2ps()
        syncing_node.disconnect_p2ps()
        network_thread_join()
        self.stop_node(serving_node.index)
        self.stop_node(syncing_node.index)
        self.log.info('test_p2p_schema passed')

    def test_sync_with_restarts(self):
        """
        This test creates the following nodes:
        1. snap_node - full node that has the the snapshot
        2. snap_p2p - mini node that is used as a helper to retrieve the snapshot content
        3. node - the node which syncs the snapshot
        4. p2p - mini node that sends snapshot in stages
        """
        snap_node = self.nodes[2]
        node = self.nodes[3]

        self.start_node(snap_node.index)
        self.start_node(node.index)

        # generate 4 blocks to create the first snapshot
        snap_node.generatetoaddress(4, snap_node.getnewaddress())
        wait_until(lambda: has_snapshot(snap_node, 3))

        # configure p2p to have snapshot header and parent block
        p2p = node.add_p2p_connection(WaitNode())
        p2p.update_snapshot_header_from(snap_node)
        p2p.update_headers_and_blocks_from(snap_node)

        # helper p2p connection to fetch snapshot content
        snap_p2p = snap_node.add_p2p_connection(BaseNode())

        network_thread_start()
        p2p.wait_for_verack()

        # fetch snapshot content for p2p
        snap_p2p.wait_for_verack()
        snap_p2p.send_message(msg_getsnaphead())
        wait_until(lambda: snap_p2p.snapshot_header.total_utxo_subsets > 0)
        getsnapshot = GetSnapshot(snap_p2p.snapshot_header.snapshot_hash, 0, snap_p2p.snapshot_header.total_utxo_subsets)
        snap_p2p.send_message(msg_getsnapshot(getsnapshot))
        wait_until(lambda: len(snap_p2p.snapshot_data) > 0)
        p2p.snapshot_data = snap_p2p.snapshot_data
        snap_node.disconnect_p2ps()

        # test 1. the node can be restarted after it discovered the snapshot
        wait_until(lambda: p2p.snapshot_chunk1_requested)
        node.disconnect_p2ps()
        network_thread_join()
        self.restart_node(node.index)
        self.log.info('Node restarted successfully after it discovered the snapshot')

        # test 2. that node can be restarted after it downloaded half of the snapshot
        p2p.return_snapshot_chunk1 = True
        node.add_p2p_connection(p2p)
        network_thread_start()
        wait_until(lambda: p2p.snapshot_chunk2_requested)
        node.disconnect_p2ps()
        network_thread_join()
        self.restart_node(node.index)
        assert_equal(len(os.listdir(os.path.join(node.datadir, "regtest", "snapshots"))), 0)
        self.log.info('Node restarted successfully after it downloaded half of the snapshot')

        # test 3. that node can be restarted after it downloaded the full snapshot
        p2p.return_snapshot_chunk2 = True
        node.add_p2p_connection(p2p)
        network_thread_start()
        wait_until(lambda: p2p.parent_block_requested)
        node.disconnect_p2ps()
        network_thread_join()
        self.restart_node(node.index)
        self.log.info('Node restarted successfully after it downloaded the full snapshot')

        # test 4. the node can be restarted after it downloaded the parent block
        p2p.return_parent_block = True
        node.add_p2p_connection(p2p)
        network_thread_start()
        wait_until(lambda: node.getblockcount() == snap_node.getblockcount())
        assert_chainstate_equal(node, snap_node)
        node.disconnect_p2ps()
        network_thread_join()
        self.restart_node(node.index)
        self.restart_node(snap_node.index)
        assert_chainstate_equal(node, snap_node)
        assert_equal(node.listsnapshots(), snap_node.listsnapshots())
        self.log.info('Node restarted successfully after it downloaded the parent block')

        # clean up test
        self.stop_node(snap_node.index)
        self.stop_node(node.index)
        self.log.info('test_sync_with_restarts passed')

    def run_test(self):
        self.stop_nodes()

        self.test_p2p_schema()
        self.test_sync_with_restarts()


if __name__ == '__main__':
    P2PSnapshotTest().main()
