#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test createsnapshot RPC"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal


class RpcCreateSnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def test_full_snapshot_creation(self):
        node = self.nodes[0]
        res = node.createsnapshot()
        keys = sorted(res.keys())
        assert_equal(keys, [
            "all_snapshot_ids",
            "best_block_hash",
            "current_snapshot_id",
            "snapshot_hash",
            "total_outputs",
            "total_utxo_subsets",
        ])
        assert_equal(res['current_snapshot_id'], 0)
        assert_equal(res['total_utxo_subsets'], 201)
        assert_equal(res['total_outputs'], 205)
        assert_equal(res['best_block_hash'], node.getbestblockhash())
        assert_equal(res['snapshot_hash'], node.readsnapshot(res['current_snapshot_id'])['snapshot_hash'])

    def test_partial_snapshot_creation(self):
        node = self.nodes[0]
        res = node.createsnapshot(10)
        assert_equal(res['current_snapshot_id'], 1)
        assert_equal(res['total_utxo_subsets'], 10)
        assert_equal(res['total_outputs'], 10)
        assert_equal(res['best_block_hash'], node.getbestblockhash())
        assert_equal(res['snapshot_hash'], node.readsnapshot(res['current_snapshot_id'])['snapshot_hash'])

    def run_test(self):
        self.test_full_snapshot_creation()
        self.test_partial_snapshot_creation()


if __name__ == '__main__':
    RpcCreateSnapshotTest().main()
