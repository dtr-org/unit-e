#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test readsnapshot RPC"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal


class RpcReadSnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [
            ["-createsnapshot=0"]
        ]
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]
        node.generatetoaddress(20, node.getnewaddress())

        res = node.readsnapshot()
        assert_equal(res['error'], "snapshot is missing")

        # read first snapshot
        node.createsnapshot(10)
        res = node.readsnapshot()
        keys = sorted(res.keys())
        assert_equal(keys, [
            "all_snapshot_ids",
            "best_block_hash",
            "current_snapshot_id",
            "snapshot_hash",
            "snapshot_id",
            "total_outputs",
            "total_utxo_subsets",
        ])
        assert_equal(res['snapshot_id'], 0)
        assert_equal(res['current_snapshot_id'], 0)
        assert_equal(res['total_utxo_subsets'], 10)

        # read new current snapshot
        node.createsnapshot(20)
        res = node.readsnapshot()
        assert_equal(res['snapshot_id'], 1)
        assert_equal(res['current_snapshot_id'], 1)
        assert_equal(res['total_utxo_subsets'], 20)

        # read previous snapshot
        res = node.readsnapshot(0)
        assert_equal(res['snapshot_id'], 0)
        assert_equal(res['current_snapshot_id'], 1)
        assert_equal(res['total_utxo_subsets'], 10)


if __name__ == '__main__':
    RpcReadSnapshotTest().main()
