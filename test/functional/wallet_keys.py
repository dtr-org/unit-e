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
        node = self.nodes[0]

        keytool = KeyTool.for_node(node)

        bech32_address = node.getnewaddress('', 'bech32')
        privkey = PrivateKey.from_node(node, bech32_address)
        pubkey = PublicKey.from_node(node, bech32_address)

        assert(pubkey.is_compressed())
        assert_equal(pubkey.to_hex(),
                     privkey.get_pubkey().to_hex())
        assert_equal(privkey.bech32_address(keytool.human_readable_prefix),
                     pubkey.bech32_address(keytool.human_readable_prefix))
        assert_equal(privkey.bech32_address(keytool.human_readable_prefix),
                     bech32_address)
        assert_equal(pubkey.bech32_address(keytool.human_readable_prefix),
                     bech32_address)

        legacy_address = node.getnewaddress('', 'legacy')
        privkey = PrivateKey.from_node(node, legacy_address)
        pubkey = PublicKey.from_node(node, legacy_address)

        assert_equal(pubkey.to_hex(),
                     privkey.get_pubkey().to_hex())
        assert_equal(privkey.legacy_address(keytool.pubkey_version_byte),
                     pubkey.legacy_address(keytool.pubkey_version_byte))
        assert_equal(privkey.legacy_address(keytool.pubkey_version_byte),
                     legacy_address)
        assert_equal(pubkey.legacy_address(keytool.pubkey_version_byte),
                     legacy_address)


if __name__ == '__main__':
    WalletKeysTest().main()
