#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test Initial Snapshot Download
"""
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
    wait_until,
)
from test_framework.messages import (
    NODE_NETWORK,
    NODE_BLOOM,
    NODE_WITNESS,
    NODE_NETWORK_LIMITED,
    NODE_SNAPSHOT,
)


class SnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True

        self.num_nodes = 6
        self.extra_args = [
            # test_service_flags, test_fast_sync
            ['-createsnapshot=0'],   # blank_node (without snapshots)
            [],                      # full_node (with snapshots)
            ['-prune=1', '-isd=1'],  # isd_node
            [],                      # rework_node (has longer chain than full_node)

            # test_fallback_to_ibd
            [],                      # full_node
            ['-prune=1', '-isd=1'],  # sync_node
        ]

    def setup_network(self):
        self.setup_nodes()

    def test_service_flags(self):
        blank_node = int(self.nodes[0].getnetworkinfo()['localservices'], 16)
        assert_equal(blank_node, NODE_NETWORK | NODE_BLOOM | NODE_WITNESS | NODE_NETWORK_LIMITED)

        full_node = int(self.nodes[1].getnetworkinfo()['localservices'], 16)
        assert_equal(full_node, NODE_NETWORK | NODE_BLOOM | NODE_WITNESS | NODE_NETWORK_LIMITED | NODE_SNAPSHOT)

        isd_node = int(self.nodes[2].getnetworkinfo()['localservices'], 16)
        assert_equal(isd_node, NODE_BLOOM | NODE_WITNESS | NODE_NETWORK_LIMITED | NODE_SNAPSHOT)

        rework_node = int(self.nodes[3].getnetworkinfo()['localservices'], 16)
        assert_equal(rework_node, NODE_NETWORK | NODE_BLOOM | NODE_WITNESS | NODE_NETWORK_LIMITED | NODE_SNAPSHOT)

        self.log.info('Test service flags passed')

    def test_fast_sync(self):
        """
        This test covers the following scenarios:
        1. node can discover the peer that has snapshot
        2. sync using snapshot
        3. after the sync, the node can accept/propose new blocks
        4. the node can switch to the fork which is created right after the snapshot
        """

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
            return res['valid']

        blank_node = self.nodes[0]
        full_node = self.nodes[1]
        isd_node = self.nodes[2]
        rework_node = self.nodes[3]

        self.setup_stake_coins(full_node, rework_node)

        # generate 2 epochs to create the snapshot which will be finalized
        # one block after
        #               s0      s1
        # G------------(h=4)...(h=9)-(h=10) full_node
        # | isd_node
        # | rework_node
        # | blank_node
        generate_block(full_node, count=5 + 5)
        assert_equal(full_node.getblockcount(), 10)
        wait_until(lambda: has_snapshot(full_node, 4), timeout=3)
        wait_until(lambda: has_snapshot(full_node, 9), timeout=3)

        # generate the longest fork that will be used later
        #               s0      s1
        # G------------(h=4)...(h=9)-(h=10) full_node
        # | isd_node                   \
        # | blank_node                  -------------------(h=15) rework_node
        connect_nodes(rework_node, full_node.index)
        sync_blocks([rework_node, full_node])
        disconnect_nodes(rework_node, full_node.index)
        generate_block(rework_node, count=5)
        assert_equal(rework_node.getblockcount(), 15)

        # generate 1 more block creates new epoch and instantly finalizes the previous one
        # to make the first snapshot be part of finalized epoch
        #               s0      s1
        # G------------(h=4)...(h=9)-(h=10)-(h=11) full_node, blank_node
        # | isd_node                   \
        #                               -------------------(h=15) rework_node
        generate_block(full_node)
        assert_equal(full_node.getblockcount(), 11)
        wait_until(lambda: has_finalized_snapshot(full_node, height=4), timeout=5)
        wait_until(lambda: has_finalized_snapshot(full_node, height=9), timeout=5)
        assert_equal(len(full_node.listsnapshots()), 2)
        connect_nodes(blank_node, full_node.index)
        sync_blocks([blank_node, full_node])
        assert_equal(len(blank_node.listsnapshots()), 0)

        # sync isd_node with blank_node and full_node using ISD
        #               s0      s1
        # G------------(h=4)...(h=9)-(h=10)-(h=11) full_node, blank_node, isd_node
        #                              \
        #                               -------------------(h=15) rework_node
        connect_nodes(isd_node, blank_node.index)
        connect_nodes(isd_node, full_node.index)
        sync_blocks([full_node, isd_node])
        wait_until(lambda: has_snapshot(isd_node, height=9), timeout=5)
        assert_equal(len(isd_node.listsnapshots()), 1)
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 11)
        assert_equal(chain['blocks'], 11)
        assert_equal(chain['initialblockdownload'], False)
        assert_equal(chain['initialsnapshotdownload'], False)
        assert_equal(chain['pruned'], True)
        assert_equal(chain['pruneheight'], 10)
        assert_equal(full_node.gettxoutsetinfo(), isd_node.gettxoutsetinfo())

        # test that isd_node can be restarted
        restart_node(isd_node)
        wait_until(lambda: isd_node.getblockcount() == 11, timeout=5)
        chain = isd_node.getblockchaininfo()
        assert_equal(chain['headers'], 11)
        assert_equal(chain['blocks'], 11)
        assert_equal(chain['initialblockdownload'], False)
        assert_equal(chain['initialsnapshotdownload'], False)

        # test that isd_node can create blocks
        #               s0      s1
        # G------------(h=4)...(h=9)-(h=10)-(h=11) full_node, blank_node
        #                              \     \
        #                               \     -(h=12) isd_node
        #                                -------------------(h=15) rework_node

        # Import funds for the node in pruning mode
        isd_node.importmasterkey(regtest_mnemonics[5]['mnemonics'], '', False)
        isd_node.initial_stake = regtest_mnemonics[5]['balance']

        genesis = isd_node.getblock(isd_node.getblockhash(0))
        funding_txid = genesis['tx'][0]
        genesis_tx_hex = isd_node.getrawtransaction(funding_txid)
        fund_proof = isd_node.gettxoutproof([funding_txid])
        isd_node.importprunedfunds(genesis_tx_hex, fund_proof)

        generate_block(isd_node)
        assert_equal(isd_node.getblockcount(), 12)

        # test that reorg one epoch after finalization is possible
        #               s0      s1
        # G------------(h=3)...(h=8)-(h=9)-(h=10)-(h=11) full_node, blank_node
        #                                    \
        #                                     ---------------(h=15) rework_node, isd_node
        connect_nodes(isd_node, rework_node.index)
        sync_blocks([isd_node, rework_node])
        assert_equal(isd_node.getblockcount(), 15)
        self.log.info('Test fast sync passed')

    def test_fallback_to_ibd(self):
        """
        This test checks that node can fallback to Initial Block Download
        if its peers can't provide the snapshot
        """
        full_node = self.nodes[4]
        sync_node = self.nodes[5]

        self.setup_stake_coins(full_node)

        generate_block(full_node, count=5)
        wait_until(lambda: len(full_node.listsnapshots()) == 1, timeout=10)
        for res in full_node.listsnapshots():
            full_node.deletesnapshot(res['snapshot_hash'])

        connect_nodes(sync_node, full_node.index)
        sync_blocks([sync_node, full_node])
        assert_equal(sync_node.gettxoutsetinfo(), full_node.gettxoutsetinfo())
        for height in range(0, 6):
            block_hash = sync_node.getblockhash(height)
            block = sync_node.getblock(block_hash)
            assert_equal(block['hash'], block_hash)

        self.log.info('Test fallback to IBD passed')

    def run_test(self):
        self.test_service_flags()
        self.test_fast_sync()
        self.test_fallback_to_ibd()


if __name__ == '__main__':
    SnapshotTest().main()
