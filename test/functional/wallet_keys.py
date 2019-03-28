#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.keytools import (
    KeyTool,
)
from test_framework.test_framework import (
    UnitETestFramework,
)
from test_framework.util import (
    assert_equal,
)


class WalletKeysTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[]]

    def test_bech32_address(self, keytool):
        self.log.info("Checking bech32 addresses...")

        self.log.info("- Get a new legacy address from the node")
        bech32_address = keytool.get_bech32_address()

        self.log.info("- Lookup the private key for the newly created address from the node")
        privkey = keytool.get_privkey(bech32_address)

        self.log.info("- Lookup the public key for the newly created address from the node")
        pubkey = keytool.get_pubkey(bech32_address)

        assert(pubkey.is_compressed())

        self.log.info("- Check that addresses derived via KeyTool match the address given by the node")
        assert_equal(keytool.get_bech32_address(privkey),
                     keytool.get_bech32_address(pubkey),
                     bech32_address)

    def test_legacy_address(self, keytool):
        self.log.info("Checking legacy addresses...")

        self.log.info("- Get a new legacy address from the node")
        legacy_address = keytool.get_legacy_address()

        self.log.info("- Lookup the private key for the newly created address from the node")
        privkey = keytool.get_privkey(legacy_address)

        self.log.info("- Lookup the public key for the newly created address from the node")
        pubkey = keytool.get_pubkey(legacy_address)

        self.log.info("- Check that addresses derived via KeyTool match the address given by the node")
        assert_equal(keytool.get_legacy_address(privkey),
                     keytool.get_legacy_address(pubkey),
                     legacy_address)

    def test_import_keys(self, keytool):
        self.log.info("Checking importing keys...")

        privkey = keytool.make_privkey()

        self.log.info("- Import private key at node")
        keytool.upload_key(privkey)

        self.log.info("- Lookup private key by bech32 address from node")
        bech32_address = keytool.get_bech32_address(privkey)
        privkey_from_node = keytool.get_privkey(bech32_address)

        self.log.info("- Check that generated private key and private key reported by node are equal")
        assert_equal(privkey, privkey_from_node)

        self.log.info("- Lookup private key by legacy address from node")
        legacy_address = keytool.get_legacy_address(privkey)
        privkey_from_node = keytool.get_privkey(legacy_address)

        self.log.info("- Check that generated private key and private key reported by node are equal")
        assert_equal(privkey, privkey_from_node)

    def run_test(self):
        keytool = KeyTool.for_node(self.nodes[0])

        self.test_bech32_address(keytool)
        self.test_legacy_address(keytool)
        self.test_import_keys(keytool)


if __name__ == '__main__':
    WalletKeysTest().main()
