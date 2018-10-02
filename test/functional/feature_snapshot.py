#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Initial Snapshot Download"""
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

    def setup_nodes(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def restart_node(node):
            self.stop_node(node.index)
            self.start_node(node.index)

        full_node = self.nodes[0]
        isd_node = self.nodes[1]
        rework_node = self.nodes[2]

        # generate 10 common blocks
        # G------------------(h=10) full_node
        # | isd_node
        # | rework_node
        full_node.generate(10)

        # generate snapshot at height=10
        assert_equal(len(full_node.createsnapshot()['snapshot_hash']), 64)

        # sync node=1 with node=0 using ISD
        # node=1 still has tip=0 but contains the snapshot that points to height=10
        # G------------------(h=10) full_node
        # | isd_node (s=10)
        # | rework_node
        restart_node(isd_node)
        connect_nodes(isd_node, full_node.index)
        wait_until(lambda: 'candidate_snapshot_id' in self.nodes[1].listsnapshots(), timeout=10)
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 10)
        assert_equal(chain['blocks'], 0)
        assert_equal(chain['initialblockdownload'], True)
        assert_equal(chain['initialsnapshotdownload'], True)
        assert_equal(chain['pruned'], True)
        assert_equal(chain['pruneheight'], 0)

        # test that isd_node can recover from the restart
        restart_node(isd_node)
        connect_nodes(isd_node, full_node.index)

        # generate one more block to move the tip for isd_node and leave ISD
        # node=1 still has tip=0 but contains the snapshot that points to height=10
        # G------------------(h=11) full_node, isd_node
        # | rework_node
        full_node.generate(1)
        sync_blocks([full_node, isd_node])
        wait_until(lambda: 'snapshot_id' in self.nodes[1].listsnapshots(), timeout=10)
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 11)
        assert_equal(chain['blocks'], 11)
        assert_equal(chain['initialblockdownload'], False)
        assert_equal(chain['initialsnapshotdownload'], False)
        assert_equal(chain['pruneheight'], 11)
        assert_equal(isd_node.gettxoutsetinfo(), full_node.gettxoutsetinfo())

        # test that isd_node can recover from the restart
        restart_node(isd_node)
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 11)
        assert_equal(chain['blocks'], 11)
        assert_equal(chain['initialblockdownload'], False)
        assert_equal(chain['initialsnapshotdownload'], False)
        connect_nodes(isd_node, full_node.index)

        # sync rework_node with full_node and create longer chain
        # that will be used later for re-work
        # G------------------(h=11) full_node, isd_node
        #                     |
        #                     x----------------------(h=25) rework_node
        connect_nodes(rework_node, full_node.index)
        sync_blocks([rework_node, full_node])
        disconnect_nodes(rework_node, full_node.index)
        rework_node.generate(14)
        assert_equal(rework_node.getblockchaininfo()['blocks'], 25)

        # test that ISD node can properly chain new blocks
        # G------------------(h=11)------------------(h=22) full_node, isd_node
        #                     |
        #                     x----------------------(h=25) rework_node
        full_node.generate(10)
        sync_blocks([full_node, isd_node])
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 21)
        assert_equal(chain['blocks'], 21)
        assert_equal(chain['pruneheight'], 11)

        # test isd_node after one more restart
        # G------------------(h=11)------------------(h=22) full_node, isd_node
        #                     |
        #                     x----------------------(h=25) rework_node
        restart_node(isd_node)
        connect_nodes(isd_node, 0)
        full_node.generate(1)
        sync_blocks([full_node, isd_node])
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 22)
        assert_equal(chain['blocks'], 22)
        assert_equal(chain['pruneheight'], 11)

        # test that rework for isd_node after the snapshot is possible
        # G------------------(h=11)------------------(h=22) full_node
        #                     |
        #                     x----------------------(h=25) rework_node, isd_node
        disconnect_nodes(isd_node, full_node.index)
        connect_nodes(isd_node, rework_node.index)
        sync_blocks([isd_node, rework_node])


if __name__ == '__main__':
    SnapshotTest().main()
