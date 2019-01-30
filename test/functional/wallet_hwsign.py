#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal


class UsbDeviceCryptoTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def run_test(self):
        hw_node, other_node = self.nodes

        try:
            hw_node.initaccountfromdevice()
        except Exception as e:
            if 'Method not found' in e.error:
                # USB support not compiled in
                return
            raise

        assert_equal(len(hw_node.listunspent()), 0)
        assert_equal(len(other_node.listunspent()), 0)

        money_addr = other_node.getnewaddress()
        other_node.generatetoaddress(102, money_addr)
        self.sync_all()

        balance = hw_node.getbalance()
        assert_equal(balance, 0)

        balance = other_node.getbalance()
        assert_equal(balance, 100)

        # Transaction with one input
        addr = hw_node.getnewaddress()
        other_node.sendtoaddress(addr, 1)
        other_node.generate(1)
        self.sync_all()

        balance = hw_node.getbalance()
        assert_equal(balance, 1)

        hw_node.sendtoaddress(other_node.getnewaddress(), 1, None, None, True)
        hw_node.generate(1)
        self.sync_all()

        # Transaction with two inputs
        other_node.sendtoaddress(addr, 0.25)
        other_node.sendtoaddress(addr, 0.25)
        other_node.generate(1)
        self.sync_all()

        balance = hw_node.getbalance()
        assert_equal(balance, 0.5)

        hw_node.sendtoaddress(other_node.getnewaddress(), 0.5, None, None, True)
        hw_node.generate(1)
        self.sync_all()
        assert(other_node.getbalance() > 99.9)


if __name__ == '__main__':
    UsbDeviceCryptoTest().main()
