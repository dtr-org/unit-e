#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test RPC calls propose and proposetoaddress

Tests correspond to code in proposer/proposer_rpc.cpp .
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
)


class RpcProposeTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0']]

    def run_test(self):
        node = self.nodes[0]
        self.setup_stake_coins(node)

        assert_equal(len(node.propose(10)), 10)
        assert_equal(node.getblockcount(), 10)

        address = node.getnewaddress('', 'bech32')

        proposed_blocks = node.proposetoaddress(5, address)
        assert_equal(len(proposed_blocks), 5)
        assert_equal(node.getblockcount(), 15)

        for i in proposed_blocks:
            txs = node.getblock(i)['tx']
            assert_equal(len(txs), 1)
            details = node.gettransaction(txs[0])['details']
            assert_equal(len(details), 1)
            assert_equal(details[0]['address'], address)


if __name__ == '__main__':
    RpcProposeTest().main()
