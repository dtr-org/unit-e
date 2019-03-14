#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework, COINBASE_MATURITY
from test_framework.util import assert_equal, sync_mempools


class UsbDeviceCryptoTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=1000000000']] * 2

    def run_test(self):
        hw_node, other_node = self.nodes
        self.setup_stake_coins(other_node)

        hw_node.initaccountfromdevice()

        assert_equal(len(hw_node.listunspent()), 0)
        assert_equal(len(other_node.listunspent()), 1)

        balance = hw_node.getbalance()
        assert_equal(balance, 0)

        other_node.generate(1)
        balance = other_node.getbalance()
        assert_equal(balance, other_node.initial_stake)

        # Transaction with one input
        addr = hw_node.getnewaddress()
        other_node.sendtoaddress(addr, 1)
        other_node.generate(1)
        self.sync_all()

        balance = hw_node.getbalance()
        assert_equal(balance, 1)

        # Send the money back
        hw_node.sendtoaddress(other_node.getnewaddress(), 1, None, None, True)
        sync_mempools(self.nodes)
        other_node.generate(1)
        self.sync_all()

        # Transaction with two inputs
        other_node.sendtoaddress(addr, 0.25)
        other_node.sendtoaddress(addr, 0.25)
        other_node.generate(1)
        self.sync_all()

        balance = hw_node.getbalance()
        assert_equal(balance, 0.5)

        # Send the money back
        hw_node.sendtoaddress(other_node.getnewaddress(), 0.5, None, None, True)
        sync_mempools(self.nodes)
        other_node.generate(1)
        self.sync_all()

        # Check final balance
        assert(other_node.getbalance() > other_node.initial_stake - 0.01)


if __name__ == '__main__':
    UsbDeviceCryptoTest().main()
