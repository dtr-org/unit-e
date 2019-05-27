#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal, assert_is_hex_string


class UsbDeviceQueryTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        node = self.nodes[0]

        # List connected devices
        devices = node.listdevices()

        assert_equal(len(devices), 1)
        assert_equal(devices[0]['vendor'], 'Debug')
        assert_equal(devices[0]['product'], 'Device')
        assert_equal(devices[0]['firmware_version'], 'debug v1')

        # Get a device's public key
        result = node.getdevicepubkey("0/0")
        assert_equal(result["path"], "m/44'/600'/0'/0/0")
        assert_is_hex_string(result["pubkey"])
        assert result["address"], "Device should return the address"

        result = node.getdevicepubkey("1/2", "m/44'/1'/0'")
        assert_equal(result["path"], "m/44'/1'/0'/1/2")

        # Get a device's extended public key
        result = node.getdeviceextpubkey("0/0")
        assert_equal(result["path"], "m/44'/600'/0'/0/0")
        assert_is_hex_string(result["extpubkey"])


if __name__ == '__main__':
    UsbDeviceQueryTest().main()
