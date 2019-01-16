#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test Snapshot Creation

This test checks the following scenarios:
1. node generates snapshots with the expected interval
2. node keeps up to 5 snapshots
3. node keeps at least 3 finalized snapshots
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    wait_until,
    sync_blocks,
)
from test_framework.admin import Admin


class SnapshotCreationTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True

        self.extra_args = [
            [
                '-validating=1',
                '-esperanzaconfig={"epochLength":5}',
            ],
            [
                '-esperanzaconfig={"epochLength":5}',
            ],
        ]
        self.num_nodes = len(self.extra_args)

    def run_test(self):
        def verify_snapshot_result(res):
            if 'snapshot_hash' not in res:
                return False
            if 'valid' not in res:
                return False
            return res['valid'] is True

        def has_valid_snapshot_for_height(node, height):
            res = node.getblocksnapshot(node.getblockhash(height))
            return verify_snapshot_result(res)

        validator = self.nodes[0]
        node = self.nodes[1]

        validator.importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        node.importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')

        node.generatetoaddress(1, node.getnewaddress())  # IBD
        Admin.authorize_and_disable(self, node)

        # test 1. node generates snapshots with the expected interval
        node.generatetoaddress(22, node.getnewaddress())
        wait_until(lambda: len(node.listsnapshots()) == 5)
        assert(has_valid_snapshot_for_height(node, 3))
        assert(has_valid_snapshot_for_height(node, 8))
        assert(has_valid_snapshot_for_height(node, 13))
        assert(has_valid_snapshot_for_height(node, 18))
        assert(has_valid_snapshot_for_height(node, 23))

        # test 2. node keeps up to 5 snapshots
        node.generatetoaddress(4, node.getnewaddress())
        wait_until(lambda: has_valid_snapshot_for_height(node, 28), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert(has_valid_snapshot_for_height(node, 3) is False)
        assert(has_valid_snapshot_for_height(node, 8))
        assert(has_valid_snapshot_for_height(node, 13))
        assert(has_valid_snapshot_for_height(node, 18))
        assert(has_valid_snapshot_for_height(node, 23))

        # disable instant finalization
        payto = validator.getnewaddress("", "legacy")
        txid = validator.deposit(payto, 10000)
        self.wait_for_transaction(txid, 10)

        node.generatetoaddress(10, node.getnewaddress())
        sync_blocks([node, validator])
        wait_until(lambda: has_valid_snapshot_for_height(node, 38), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert(has_valid_snapshot_for_height(node, 8) is False)
        assert(has_valid_snapshot_for_height(node, 13) is False)
        assert(node.getblocksnapshot(node.getblockhash(18))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(23))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(28))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(33))['snapshot_finalized'] is False)  # will be finalized
        assert(node.getblocksnapshot(node.getblockhash(38))['snapshot_finalized'] is False)  # will be finalized

        # test 3. node keeps at least 3 finalized snapshots
        node.generatetoaddress(10, node.getnewaddress())
        wait_until(lambda: has_valid_snapshot_for_height(node, 48), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert(has_valid_snapshot_for_height(node, 18) is False)
        assert(has_valid_snapshot_for_height(node, 23) is False)
        assert(node.getblocksnapshot(node.getblockhash(28))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(33))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(38))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(43))['snapshot_finalized'] is False)
        assert(node.getblocksnapshot(node.getblockhash(48))['snapshot_finalized'] is False)

        node.generatetoaddress(5, node.getnewaddress())
        wait_until(lambda: has_valid_snapshot_for_height(node, 53), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert(node.getblocksnapshot(node.getblockhash(28))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(33))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(38))['snapshot_finalized'])
        assert(has_valid_snapshot_for_height(node, 43) is False)
        assert(node.getblocksnapshot(node.getblockhash(48))['snapshot_finalized'] is False)

        node.generatetoaddress(5, node.getnewaddress())
        wait_until(lambda: has_valid_snapshot_for_height(node, 58), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert(node.getblocksnapshot(node.getblockhash(28))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(33))['snapshot_finalized'])
        assert(node.getblocksnapshot(node.getblockhash(38))['snapshot_finalized'])
        assert(has_valid_snapshot_for_height(node, 48) is False)
        assert(node.getblocksnapshot(node.getblockhash(53))['snapshot_finalized'] is False)


if __name__ == '__main__':
    SnapshotCreationTest().main()
