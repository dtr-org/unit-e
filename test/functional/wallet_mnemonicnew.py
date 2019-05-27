#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test mnemonicinfo rpc command and key derivation from seed according to TREZOR test vectors."""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
)


class WalletMnemonicNewTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]

    def run_test(self):
        mnemonic = self.nodes[0].mnemonic('new')

        info = self.nodes[0].mnemonic('info', mnemonic['mnemonic'])

        assert_equal(mnemonic['master'], info['bip32_root'])
        assert_equal(info['language_tag'], 'english')


if __name__ == '__main__':
    WalletMnemonicNewTest().main()
