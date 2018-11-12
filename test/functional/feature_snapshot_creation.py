#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test Snapshot Creation

This test starts two nodes:
node0 is used to create snapshots and to verify that they point to the right block height
node1 syncs with node0 using fast sync to verify the content of the snapshot
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    wait_until,
    sync_blocks,
    connect_nodes,
)
import time


class SnapshotCreationTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [
            [],
            ['-prune=1', '-isd=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        node0 = self.nodes[0]

        # no snapshot
        #  ---- epoch 0 ------
        # /                   \
        # b0 - b1 - b2
        node0.generatetoaddress(2, node0.getnewaddress())
        time.sleep(1)
        assert('snapshot_id' not in node0.readsnapshot())

        # trigger snapshot creation
        #  ---- epoch 0 ------
        # /                   \
        # b0 - b1 - b2 - b3
        #                \
        #                 s0
        node0.generatetoaddress(1, node0.getnewaddress())
        wait_until(lambda: 'snapshot_hash' in node0.readsnapshot(0), timeout=5)
        assert_equal(node0.readsnapshot(0)['best_block_hash'], node0.getbestblockhash())

        # no new snapshots in the middle of the new epoch
        #  ---- epoch 0 ------       ---- epoch 1 ------
        # /                   \     /                   \
        # b0 - b1 - b2 - b3 - b4 - b5 - b6 - b7
        #                \
        #                 s0
        node0.generatetoaddress(4, node0.getnewaddress())
        time.sleep(1)
        assert_equal(node0.readsnapshot().get('snapshot_id'), 0)

        # generate second snapshot
        #  ---- epoch 0 ------       ---- epoch 1 ------
        # /                   \     /                   \
        # b0 - b1 - b2 - b3 - b4 - b5 - b6 - b7 - b8
        #                \                        \
        #                 s0                       s1
        node0.generatetoaddress(1, node0.getnewaddress())
        wait_until(lambda: 'snapshot_hash' in node0.readsnapshot(1), timeout=5)
        assert_equal(node0.readsnapshot(1)['best_block_hash'], node0.getbestblockhash())

        # generate 3 more snapshots
        #  - epoch 0 -       - epoch 1 -     -- epoch 2 --      -- epoch 3 --       -- epoch 4 --
        # /           \     /           \   /             \    /             \     /             \
        # ...   - b4 - b5 - ...   - b8 - b9 - ...  - b13 - b14 - ...  - b18 - b19 - ...  - b23
        #          \                 \                \                  \                  \
        #           s0                s1               s2                 s3                s4
        node0.generatetoaddress(15, node0.getnewaddress())
        wait_until(lambda: 'snapshot_hash' in node0.readsnapshot(4), timeout=5)
        assert_equal(node0.readsnapshot(4)['best_block_hash'], node0.getbestblockhash())

        # keep the last 5 snapshots
        #   - epoch 1 -      -- epoch 2 --      -- epoch 3 --      -- epoch 4 --     -- epoch 5 --
        # /            \    /             \    /             \    /             \   /             \
        # ...   - b8 - b9 - ...  - b13 - b14 - ...  - b18 - b19 - ...  - b23 - b24 - ... - b28
        #          \                 \                \                  \                  \
        #           s1                s2               s3                 s4                s5
        node0.generatetoaddress(5, node0.getnewaddress())
        wait_until(lambda: 'snapshot_hash' in node0.readsnapshot(5), timeout=5)
        assert_equal(node0.readsnapshot(5)['best_block_hash'], node0.getbestblockhash())
        assert('error' in node0.readsnapshot(0))

        # verify the content of the latest snapshot
        node0.generatetoaddress(1, node0.getnewaddress())
        node1 = self.nodes[1]
        connect_nodes(node1, node0.index)
        sync_blocks(self.nodes)
        node0_snapshot = node0.readsnapshot(5)
        node1_snapshot = node1.readsnapshot(0)
        assert('error' in node1.readsnapshot(1))
        assert_equal(node0_snapshot['snapshot_hash'], node1_snapshot['snapshot_hash'])
        assert_equal(node0_snapshot['best_block_hash'], node1_snapshot['best_block_hash'])
        assert_equal(node0.gettxoutsetinfo(), node1.gettxoutsetinfo())


if __name__ == '__main__':
    SnapshotCreationTest().main()
