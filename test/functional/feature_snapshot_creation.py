#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test Snapshot Creation

This test checks the following scenarios:
1. node generates snapshots with the expected interval
2. node keeps up to 5 snapshots
3. node keeps at least 3 finalized snapshots
"""
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    generate_block,
    wait_until,
)


class SnapshotCreationTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True

        self.extra_args = [
            ['-validating=1'],
            [],
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

        validator.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        node.importmasterkey(regtest_mnemonics[1]['mnemonics'])

        generate_block(node)  # IBD

        # test 1. node generates snapshots with the expected interval
        generate_block(node, count=23)
        wait_until(lambda: len(node.listsnapshots()) == 5)
        assert has_valid_snapshot_for_height(node, 4)
        assert has_valid_snapshot_for_height(node, 9)
        assert has_valid_snapshot_for_height(node, 14)
        assert has_valid_snapshot_for_height(node, 19)
        assert has_valid_snapshot_for_height(node, 24)

        # test 2. node keeps up to 5 snapshots
        generate_block(node, count=5)
        assert_equal(node.getblockcount(), 29)
        wait_until(lambda: has_valid_snapshot_for_height(node, 29), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert has_valid_snapshot_for_height(node, 4) is False
        assert has_valid_snapshot_for_height(node, 9)
        assert has_valid_snapshot_for_height(node, 14)
        assert has_valid_snapshot_for_height(node, 19)
        assert has_valid_snapshot_for_height(node, 24)

        # disable instant justification
        payto = validator.getnewaddress("", "legacy")
        txid = validator.deposit(payto, 1500)
        self.wait_for_transaction(txid, 10)
        self.stop_node(validator.index)

        generate_block(node, count=12)
        assert_equal(node.getblockcount(), 41)
        assert_finalizationstate(node, {'currentDynasty': 6,
                                        'currentEpoch': 9,
                                        'lastJustifiedEpoch': 6,
                                        'lastFinalizedEpoch': 6,
                                        'validators': 1})

        wait_until(lambda: has_valid_snapshot_for_height(node, 39), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert has_valid_snapshot_for_height(node, 9) is False
        assert has_valid_snapshot_for_height(node, 14) is False
        assert node.getblocksnapshot(node.getblockhash(19))['snapshot_finalized']
        assert node.getblocksnapshot(node.getblockhash(24))['snapshot_finalized']
        assert node.getblocksnapshot(node.getblockhash(29))['snapshot_finalized']
        assert node.getblocksnapshot(node.getblockhash(34))['snapshot_finalized'] is False
        assert node.getblocksnapshot(node.getblockhash(39))['snapshot_finalized'] is False

        # test 3. node keeps at least 2 finalized snapshots
        generate_block(node, count=9)
        wait_until(lambda: has_valid_snapshot_for_height(node, 49), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert has_valid_snapshot_for_height(node, 19) is False
        assert node.getblocksnapshot(node.getblockhash(24))['snapshot_finalized']
        assert node.getblocksnapshot(node.getblockhash(29))['snapshot_finalized']
        assert has_valid_snapshot_for_height(node, 34) is False
        assert node.getblocksnapshot(node.getblockhash(39))['snapshot_finalized'] is False
        assert node.getblocksnapshot(node.getblockhash(44))['snapshot_finalized'] is False
        assert node.getblocksnapshot(node.getblockhash(49))['snapshot_finalized'] is False

        generate_block(node, count=5)
        wait_until(lambda: has_valid_snapshot_for_height(node, 54), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert node.getblocksnapshot(node.getblockhash(24))['snapshot_finalized']
        assert node.getblocksnapshot(node.getblockhash(29))['snapshot_finalized']
        assert has_valid_snapshot_for_height(node, 39) is False
        assert node.getblocksnapshot(node.getblockhash(44))['snapshot_finalized'] is False
        assert node.getblocksnapshot(node.getblockhash(49))['snapshot_finalized'] is False

        generate_block(node, count=5)
        wait_until(lambda: has_valid_snapshot_for_height(node, 59), timeout=10)
        assert_equal(len(node.listsnapshots()), 5)
        assert node.getblocksnapshot(node.getblockhash(24))['snapshot_finalized']
        assert node.getblocksnapshot(node.getblockhash(29))['snapshot_finalized']
        assert has_valid_snapshot_for_height(node, 44) is False
        assert node.getblocksnapshot(node.getblockhash(49))['snapshot_finalized'] is False
        assert node.getblocksnapshot(node.getblockhash(54))['snapshot_finalized'] is False


if __name__ == '__main__':
    SnapshotCreationTest().main()
