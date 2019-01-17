#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test getblocksnapshot RPC

This test covers the following use cases:
1. getting data about deleted snapshot
2. getting snapshot data about the block which never had a snapshot
2. getting data about non finalized snapshot
3. getting data about finalized snapshot
4. getting snapshot data for the block of the fork
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    sync_blocks,
    disconnect_nodes,
)


class RpcGetBlockSnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def run_test(self):
        def assert_deleted_snapshot(node, height):
            block_hash = node.getblockhash(height)
            res = node.getblocksnapshot(block_hash)
            keys = sorted(res.keys())
            assert_equal(keys, [
                'block_hash',
                'snapshot_deleted',
                'snapshot_hash',
                'valid',
            ])
            assert_equal(res['valid'], False)
            assert_equal(res['snapshot_deleted'], True)
            assert_equal(res['block_hash'], block_hash)
            assert_equal(len(res['snapshot_hash']), 64)

        def assert_valid_finalized(node, height):
            block_hash = node.getblockhash(height)
            res = node.getblocksnapshot(block_hash)
            keys = sorted(res.keys())
            assert_equal(keys, [
                'block_hash',
                'block_height',
                'chain_work',
                'snapshot_finalized',
                'snapshot_hash',
                'stake_modifier',
                'total_outputs',
                'total_utxo_subsets',
                'valid',
            ])
            assert_equal(res['valid'], True)
            assert_equal(res['block_hash'], block_hash)
            assert_equal(res['block_height'], height)
            assert_equal(res['snapshot_finalized'], True)
            assert_equal(len(res['snapshot_hash']), 64)
            assert_equal(len(res['stake_modifier']), 64)

        def assert_valid_non_finalized(node, height):
            block_hash = node.getblockhash(height)
            res = node.getblocksnapshot(block_hash)
            keys = sorted(res.keys())
            assert_equal(keys, [
                'block_hash',
                'block_height',
                'chain_work',
                'snapshot_finalized',
                'snapshot_hash',
                'stake_modifier',
                'total_outputs',
                'total_utxo_subsets',
                'valid',
            ])
            assert_equal(res['valid'], True)
            assert_equal(res['block_hash'], block_hash)
            assert_equal(res['block_height'], height)
            assert_equal(res['snapshot_finalized'], False)
            assert_equal(len(res['snapshot_hash']), 64)
            assert_equal(len(res['stake_modifier']), 64)

        # generate two forks that are available for node0
        # 0 ... 5 ... 10 ... 25
        #       \
        #         ... 10
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node0.generatetoaddress(5, node0.getnewaddress())
        connect_nodes(node1, node0.index)
        sync_blocks([node0, node1])
        forked_block_hash = node1.generatetoaddress(1, node1.getnewaddress())[0]
        node1.generatetoaddress(4, node1.getnewaddress())
        sync_blocks([node0, node1])
        disconnect_nodes(node1, node0.index)
        node0.generatetoaddress(20, node0.getnewaddress())

        assert_deleted_snapshot(node0, height=3)  # actually deleted
        assert_deleted_snapshot(node0, height=4)  # wasn't created
        assert_valid_finalized(node0, height=8)
        assert_valid_non_finalized(node0, height=28)

        res = node0.getblocksnapshot(forked_block_hash)
        assert_equal(res['valid'], False)
        assert_equal(res['snapshot_deleted'], True)
        assert_equal(res['block_hash'], forked_block_hash)
        assert_equal(len(res['snapshot_hash']), 64)


if __name__ == '__main__':
    RpcGetBlockSnapshotTest().main()
