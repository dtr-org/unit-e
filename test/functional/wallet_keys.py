#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.keytools import (
    KeyTool,
    PublicKey,
    PrivateKey,
)
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
)


class WalletKeysTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]

    def run_test(self):
        keytool = KeyTool.for_node(self.nodes[0])

        bech32_address = keytool.get_bech32_address()
        privkey = keytool.get_privkey(bech32_address)
        pubkey = keytool.get_pubkey(bech32_address)

        assert(pubkey.is_compressed())

        assert_equal(pubkey.to_hex(),
                     privkey.get_pubkey().to_hex())
        assert_equal(keytool.get_bech32_address(privkey),
                     keytool.get_bech32_address(pubkey),
                     bech32_address)

        legacy_address = keytool.get_legacy_address()
        privkey = keytool.get_privkey(legacy_address)
        pubkey = keytool.get_pubkey(legacy_address)

        assert_equal(pubkey.to_hex(),
                     privkey.get_pubkey().to_hex())
        assert_equal(keytool.get_legacy_address(privkey),
                     keytool.get_legacy_address(pubkey),
                     legacy_address)


if __name__ == '__main__':
    WalletKeysTest().main()
