#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test Initial Snapshot Download

This test covers the following scenarios:
1. sync using snapshot
2. after the sync, the node can accept/propose new blocks
3. the node can switch to the fork which is created right after the snapshot
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    sync_blocks,
    wait_until,
)


class SnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True

        # 0 - full node
        # 1 - ISD node uses full node to sync
        # 2 - rework node that has better chain than full node
        self.num_nodes = 3

        self.extra_args = [
            [],
            ['-prune=1', '-isd=1'],
            [],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def restart_node(node):
            self.stop_node(node.index)
            self.start_node(node.index)

        def has_finalized_snapshot(node, height):
            res = node.getblocksnapshot(node.getblockhash(height))
            if 'valid' not in res:
                return False
            if 'snapshot_finalized' not in res:
                return False
            return True

        def has_snapshot(node, height):
            res = node.getblocksnapshot(node.getblockhash(height))
            if 'valid' not in res:
                return False
            return True

        full_node = self.nodes[0]
        isd_node = self.nodes[1]
        rework_node = self.nodes[2]

        # generate 3 blocks to create first snapshot and its parent block
        #               s0
        # G------------(h=3)-(h=4) full_node
        # | isd_node
        # | rework_node
        full_node.generatetoaddress(4, full_node.getnewaddress())
        wait_until(lambda: has_snapshot(full_node, 3), timeout=3)

        # generate the longest fork that will be used later
        #               s0
        # G------------(h=3)-(h=4) full_node
        # | isd_node          \
        #                      --------------------------(h=20) rework_node
        connect_nodes(rework_node, full_node.index)
        sync_blocks([rework_node, full_node])
        disconnect_nodes(rework_node, full_node.index)
        rework_node.generatetoaddress(16, rework_node.getnewaddress())
        assert_equal(rework_node.getblockchaininfo()['blocks'], 20)

        # generate 6 more blocks to make the first snapshot finalized
        #               s0             s1
        # G------------(h=3)-(h=4)----(h=8)-(h=9) full_node
        # | isd_node          \
        #                      --------------------------(h=20) rework_node
        full_node.generatetoaddress(5, full_node.getnewaddress())
        wait_until(lambda: has_finalized_snapshot(full_node, height=3), timeout=5)
        assert_equal(len(full_node.listsnapshots()), 2)

        # sync node=1 with node=0 using ISD
        #               s0             s1
        # G------------(h=3)-(h=4)----(h=8)-(h=9) full_node, isd_node
        #                     \
        #                      --------------------------(h=20) rework_node
        connect_nodes(isd_node, full_node.index)
        sync_blocks([full_node, isd_node])
        assert_equal(full_node.listsnapshots(), isd_node.listsnapshots())
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 9)
        assert_equal(chain['blocks'], 9)
        assert_equal(chain['initialblockdownload'], False)
        assert_equal(chain['initialsnapshotdownload'], False)
        assert_equal(chain['pruned'], True)
        assert_equal(chain['pruneheight'], 4)
        assert_equal(full_node.gettxoutsetinfo(), isd_node.gettxoutsetinfo())

        # test that isd_node can be restarted
        restart_node(isd_node)
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 9)
        assert_equal(chain['blocks'], 9)
        assert_equal(chain['initialblockdownload'], False)
        assert_equal(chain['initialsnapshotdownload'], False)

        # test that isd_node can create blocks
        #               s0             s1
        # G------------(h=3)-(h=4)----(h=8)-(h=9) full_node
        #                     \              \
        #                      \              --(h=10) isd_node
        #                       ------------------------(h=20) rework_node
        isd_node.generatetoaddress(1, isd_node.getnewaddress())

        # test that rework after the snapshot is possible
        #               s0             s1
        # G------------(h=3)-(h=4)----(h=8)-(h=9) full_node
        #                     \
        #                      --------------------------(h=20) rework_node, isd_node
        connect_nodes(isd_node, rework_node.index)
        sync_blocks([isd_node, rework_node])


if __name__ == '__main__':
    SnapshotTest().main()
