#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test Snapshot Creation

This test starts three nodes:
node0 is used to create snapshots and to verify that they point to the right block height
node1 syncs with node0 using fast sync to verify the content of the snapshot
node2 creates larger fork and node0 must create a snapshot for this fork too
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    wait_until,
    sync_blocks,
    connect_nodes,
    disconnect_nodes,
)
import time


class SnapshotCreationTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 3
        self.extra_args = [
            [],
            ['-prune=1', '-isd=1'],
            [],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def verify_snapshot_result(res):
            if 'snapshot_hash' not in res:
                return False
            if 'valid' not in res:
                return False
            return res['valid'] is True

        def verify_snapshot_for(node, block_hash):
            res = node.getblocksnapshot(block_hash)
            return verify_snapshot_result(res)

        def assert_valid_snapshots(node, num):
            res = node.listsnapshots()
            assert_equal(len(res), num)
            for v in res:
                assert(verify_snapshot_result(v))

        node0 = self.nodes[0]
        node2 = self.nodes[2]

        # no snapshot
        # +---- epoch 0 ------+
        # |                   |
        # b0 - b1 - b2
        node0.generatetoaddress(2, node0.getnewaddress())
        time.sleep(1)
        assert_valid_snapshots(node0, 0)

        # trigger snapshot creation
        # +---- epoch 0 ------+
        # |                   |
        # b0 - b1 - b2 - b3
        #                s0
        node0.generatetoaddress(1, node0.getnewaddress())
        wait_until(lambda: verify_snapshot_for(node0, node0.getbestblockhash()), timeout=5)

        # no new snapshots in the middle of the new epoch
        # +---- epoch 0 ------+    +------ epoch 1 -----+
        # |                   |    |                    |
        # b0 - b1 - b2 - b3 - b4 - b5 - b6 - b7
        #                s0
        node0.generatetoaddress(4, node0.getnewaddress())
        time.sleep(1)
        assert_valid_snapshots(node0, 1)

        # create alternative fork that will be used later
        # b0 - b1 - b2 - b3 - b4 - b5 - b6 - b7 (node0)
        #                                     \
        #                                      +- ... - a29 (node2)
        connect_nodes(node2, node0.index)
        sync_blocks([node0, node2])
        disconnect_nodes(node2, node0.index)
        node2.generatetoaddress(24, node2.getnewaddress())

        # generate second snapshot
        # +---- epoch 0 ------+    +------ epoch 1 -----+
        # |                   |    |                    |
        # b0 - b1 - b2 - b3 - b4 - b5 - b6 - b7 - b8
        #                s0                       s1
        node0.generatetoaddress(1, node0.getnewaddress())
        wait_until(lambda: verify_snapshot_for(node0, node0.getbestblockhash()), timeout=5)

        # generate 3 more snapshots
        # +- epoch 0 -+    +- epoch 1 -+     +-- epoch 2 -+     +-- epoch 3 --+     +-- epoch 4 --+
        # |           |    |           |     |            |     |             |     |             |
        # b0 ... b3 - b4 - b5 ... b8 - b9 - b10 ... b13 - b14 - b15 ... b18 - b19 - b20 ... b23
        #        s0               s1                s3                  s4                  s4
        node0.generatetoaddress(15, node0.getnewaddress())
        wait_until(lambda: verify_snapshot_for(node0, node0.getbestblockhash()), timeout=5)
        assert_valid_snapshots(node0, 5)

        # keeps only 5 snapshots (preserving all finalized)
        #   +- epoch 1 -+     +-- epoch 2 -+     +-- epoch 3 --+     +-- epoch 4 --+     +-- epoch 4 --+
        #   |           |     |            |     |             |     |             |     |             |
        # - b5 ... b8 - b9 - b10 ... b13 - b14 - b15 ... b18 - b19 - b20 ... b23 - b24 - b25 ... b28
        #          s1                s3                  s4                  s4                  s5
        node0.generatetoaddress(5, node0.getnewaddress())
        wait_until(lambda: verify_snapshot_for(node0, node0.getbestblockhash()), timeout=5)
        assert_valid_snapshots(node0, 5)
        assert(verify_snapshot_for(node0, node0.getblockhash(3)) is False)
        assert(verify_snapshot_for(node0, node0.getblockhash(8)))
        assert(verify_snapshot_for(node0, node0.getblockhash(13)))
        assert(verify_snapshot_for(node0, node0.getblockhash(18)))
        assert(verify_snapshot_for(node0, node0.getblockhash(23)))
        assert(verify_snapshot_for(node0, node0.getblockhash(28)))

        # verify the content of the latest finalized snapshot (s4)
        node0.generatetoaddress(1, node0.getnewaddress())
        wait_until(lambda: node0.getblocksnapshot(node0.getblockhash(23))['snapshot_finalized'] is True, timeout=5)
        node1 = self.nodes[1]
        connect_nodes(node1, node0.index)
        sync_blocks([node0, node1])
        node0_snapshots = node0.listsnapshots()
        node1_snapshots = node1.listsnapshots()
        assert_equal(len(node1_snapshots), 2)
        assert_equal(node0_snapshots[-2], node1_snapshots[0])
        assert_equal(node0_snapshots[-1], node1_snapshots[1])
        assert_equal(node0.gettxoutsetinfo(), node1.gettxoutsetinfo())
        disconnect_nodes(node1, node0.index)

        # test that after switching to the alternative fork node0 creates snapshots for it and keeps original ones
        # +---- epoch 1 ------+     +-- epoch 2 -+     +-- epoch 3 --+     +-- epoch 4 --+     +-- epoch 4 --+
        # |                   |     |            |     |             |     |             |     |             |
        # b5 - b6 - b7 - b8 - b9 - b10 ... b13 - b14 - b15 ... b18 - b19 - b20 ... b23 - b24 - b25 ... b28 - b29
        #           |    s1                s3                  s4                  s4                  s5
        #           |
        #           +- - a8 - a9 - a10 ... a13 - a14 - a15 ... a18 - a19 - a20 ... a23 - a24 - a25 ... a28 - a29 - a30
        #                s6                s7                  s8                  s9                  s10
        connect_nodes(node0, node2.index)
        sync_blocks([node0, node2])
        for s in node0_snapshots:
            assert(node0.getblocksnapshot(s['block_hash'])['snapshot_deleted'])
        new_node0_snapshots = node0.listsnapshots()
        assert_equal(len(new_node0_snapshots), 5)
        for s in new_node0_snapshots:
            assert(verify_snapshot_result(s))
        assert_equal(new_node0_snapshots[0]['block_hash'], node0.getblockhash(8))
        assert_equal(new_node0_snapshots[1]['block_hash'], node0.getblockhash(13))
        assert_equal(new_node0_snapshots[2]['block_hash'], node0.getblockhash(18))
        assert_equal(new_node0_snapshots[3]['block_hash'], node0.getblockhash(23))
        assert_equal(new_node0_snapshots[4]['block_hash'], node0.getblockhash(28))


if __name__ == '__main__':
    SnapshotCreationTest().main()
