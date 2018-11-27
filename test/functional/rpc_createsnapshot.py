#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test createsnapshot RPC"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal


class RpcCreateSnapshotTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [
            ['-createsnapshot=0']
        ]
        self.setup_clean_chain = True

    def test_full_snapshot_creation(self):
        node = self.nodes[0]
        node.generatetoaddress(50, node.getnewaddress())

        res = node.createsnapshot()
        keys = sorted(res.keys())
        assert_equal(keys, [
            'block_hash',
            'block_height',
            'snapshot_hash',
            'stake_modifier',
            'total_outputs',
            'total_utxo_subsets',
            'valid'
        ])
        assert_equal(res['valid'], True)
        assert_equal(res['block_hash'], node.getbestblockhash())
        assert_equal(res['snapshot_hash'], node.getblocksnapshot(node.getbestblockhash())['snapshot_hash'])
        assert_equal(res['total_utxo_subsets'], 51)
        assert_equal(res['total_outputs'], 155)

    def test_partial_snapshot_creation(self):
        node = self.nodes[0]
        node.generatetoaddress(1, node.getnewaddress())
        res = node.createsnapshot(10)
        assert_equal(res['valid'], True)
        assert_equal(res['block_hash'], node.getbestblockhash())
        assert_equal(res['snapshot_hash'], node.getblocksnapshot(node.getbestblockhash())['snapshot_hash'])
        assert_equal(res['total_utxo_subsets'], 10)
        assert_equal(res['total_outputs'], 10)

    def run_test(self):
        self.test_full_snapshot_creation()
        self.test_partial_snapshot_creation()


if __name__ == '__main__':
    RpcCreateSnapshotTest().main()
