#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal


class UsbDeviceCryptoTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def run_test(self):
        node0, node1, node2 = self.nodes

        node0.initaccountfromdevice()
        node1.initaccountfromdevice()

        assert_equal(len(self.nodes[0].listunspent()), 0)
        assert_equal(len(self.nodes[1].listunspent()), 0)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        self.nodes[2].generate(101)

        walletinfo = self.nodes[2].getwalletinfo()
        assert_equal(walletinfo['balance'], 50)
        self.sync_all()

        balance = node2.getbalance()
        assert_equal(balance, 50)

        balance = node1.getbalance()
        assert_equal(balance, 0)

        for address_type in ['legacy', 'p2sh-segwit', 'bech32']:
            addr = node0.getnewaddress(None, address_type)
            node2.sendtoaddress(addr, 1)
            node2.generate(1)
            self.sync_all()

            balance = node0.getbalance()
            assert_equal(balance, 1)

            node0.sendtoaddress(node2.getnewaddress(), 1, None, None, True)
            node0.generate(1)
            self.sync_all()


if __name__ == '__main__':
    UsbDeviceCryptoTest().main()
