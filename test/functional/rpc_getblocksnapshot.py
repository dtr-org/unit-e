#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
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
    wait_until,
)


class RpcGetBlockSnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def wait_for_deleted_snapshot(node, height):
            def check():
                block_hash = node.getblockhash(height)
                res = node.getblocksnapshot(block_hash)
                keys = sorted(res.keys())
                expected_keys = [
                    'block_hash',
                    'snapshot_deleted',
                    'snapshot_hash',
                    'valid',
                ]
                if keys != expected_keys:
                    return False
                return all([
                    res['valid'] == False,
                    res['snapshot_deleted'] == True,
                    res['block_hash'] == block_hash,
                    len(res['snapshot_hash']) == 64,
                ])
            wait_until(check)

        def wait_for_valid_finalized(node, height):
            def check():
                block_hash = node.getblockhash(height)
                res = node.getblocksnapshot(block_hash)
                keys = sorted(res.keys())
                expected_keys = [
                    'block_hash',
                    'block_height',
                    'chain_work',
                    'snapshot_finalized',
                    'snapshot_hash',
                    'stake_modifier',
                    'total_outputs',
                    'total_utxo_subsets',
                    'valid',
                ]
                if keys != expected_keys:
                    return False
                return all([
                    res['valid'] == True,
                    res['block_hash'] == block_hash,
                    res['block_height'] == height,
                    res['snapshot_finalized'] == True,
                    len(res['snapshot_hash']) == 64,
                    len(res['stake_modifier']) == 64,
                    len(res['chain_work']) == 64,
                ])
            wait_until(check)

        def wait_for_valid_non_finalized(node, height):
            def check():
                block_hash = node.getblockhash(height)
                res = node.getblocksnapshot(block_hash)
                keys = sorted(res.keys())
                expected_keys = [
                    'block_hash',
                    'block_height',
                    'chain_work',
                    'snapshot_finalized',
                    'snapshot_hash',
                    'stake_modifier',
                    'total_outputs',
                    'total_utxo_subsets',
                    'valid',
                ]
                if keys != expected_keys:
                    return False
                return all([
                    res['valid'] == True,
                    res['block_hash'] == block_hash,
                    res['block_height'] == height,
                    res['snapshot_finalized'] == False,
                    len(res['snapshot_hash']) == 64,
                    len(res['stake_modifier']) == 64,
                    len(res['chain_work']) == 64,
                ])
            wait_until(check)

        # generate two forks that are available for node0
        # 0 ... 7 ... 10 ... 29 node2
        #       \
        #         ... 8 node1
        node0 = self.nodes[0]  # test node that switches between forks
        node1 = self.nodes[1]  # shorter chain
        node2 = self.nodes[2]  # longer chain

        self.setup_stake_coins(node0, node1, node2)

        node0.generatetoaddress(7, node0.getnewaddress('', 'bech32'))
        connect_nodes(node1, node0.index)
        connect_nodes(node2, node0.index)
        sync_blocks([node0, node1])
        sync_blocks([node0, node2])

        disconnect_nodes(node1, node0.index)
        disconnect_nodes(node2, node0.index)

        # generated shorter fork
        forked_block_hash = node1.generatetoaddress(1, node1.getnewaddress('', 'bech32'))[0]
        connect_nodes(node0, node1.index)
        sync_blocks([node0, node1])
        disconnect_nodes(node0, node1.index)
        assert_equal(node0.getblockcount(), 8)
        assert_equal(node0.getblockhash(node0.getblockcount()), forked_block_hash)

        # generate longer fork
        node2.generatetoaddress(22, node2.getnewaddress('', 'bech32'))
        connect_nodes(node0, node2.index)
        sync_blocks([node0, node2])
        disconnect_nodes(node0, node2.index)

        # make sure the node generated snapshots up to expected height
        wait_until(lambda: 'valid' in node0.getblocksnapshot(node0.getblockhash(28)), timeout=10)

        wait_for_deleted_snapshot(node0, height=3)  # actually deleted
        wait_for_deleted_snapshot(node0, height=4)  # wasn't created
        wait_for_valid_finalized(node0, height=8)
        wait_for_valid_non_finalized(node0, height=28)

        res = node0.getblocksnapshot(forked_block_hash)
        assert_equal(res['error'], "can't retrieve snapshot hash of the fork")


if __name__ == '__main__':
    RpcGetBlockSnapshotTest().main()
