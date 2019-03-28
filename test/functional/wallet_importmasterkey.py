#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test importing a HD masterkey from a seed value (BIP39)."""

from glob import glob

from test_framework.test_framework import UnitETestFramework
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.util import assert_equal, assert_raises_rpc_error, sync_blocks, connect_nodes

class WalletImportmasterkeyTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.extra_args = [[], [], [], [], ["-prune=1"]]
        self.setup_clean_chain = True

    _seed = 'tongue man magnet bacon galaxy enrich cram globe invest steel undo half nature present lend'
    _passphrase = 'crazy horse battery staple'

    @property
    def backup_count(self):
        return len(glob('%s/regtest/wallet.dat~*' % self.nodes[0].datadir))

    def test_import_consistency(self):

        self.log.info("Test that importing tha same key on different nodes leads to the same results ")

        old_backup_count = self.backup_count

        result = self.nodes[0].importmasterkey(self._seed, self._passphrase)
        assert_equal(result['success'], True)

        result = self.nodes[1].importmasterkey(self._seed, self._passphrase)
        assert_equal(result['success'], True)

        # importmasterkey should create the backups in the datadir
        assert_equal(self.backup_count, old_backup_count + 1)

        # generate a bunch of addresses on both nodes
        node0_address0 = self.nodes[0].getnewaddress()
        node0_address1 = self.nodes[0].getnewaddress()
        node0_address2 = self.nodes[0].getnewaddress()
        node0_address3 = self.nodes[0].getnewaddress()
        node0_address4 = self.nodes[0].getnewaddress()

        node1_address0 = self.nodes[1].getnewaddress()
        node1_address1 = self.nodes[1].getnewaddress()
        node1_address2 = self.nodes[1].getnewaddress()
        node1_address3 = self.nodes[1].getnewaddress()
        node1_address4 = self.nodes[1].getnewaddress()

        # checks that both nodes generate the same keys from the same seed
        assert_equal(node0_address0, node1_address0)
        assert_equal(node0_address1, node1_address1)
        assert_equal(node0_address2, node1_address2)
        assert_equal(node0_address3, node1_address3)
        assert_equal(node0_address4, node1_address4)

        reservekeys1 = self.nodes[0].listreservekeys()
        reservekeys2 = self.nodes[1].listreservekeys()

        assert_equal(reservekeys1, reservekeys2)

        # checks that the key was imported, saved, and recovered across restart
        self.stop_node(1)
        self.start_node(1)

        node0_address5 = self.nodes[0].getnewaddress()
        node0_address6 = self.nodes[0].getnewaddress()

        node1_address5 = self.nodes[1].getnewaddress()
        node1_address6 = self.nodes[1].getnewaddress()

        assert_equal(node0_address5, node1_address5)
        assert_equal(node0_address6, node1_address6)

    def test_rescan_with_pruning(self):

        self.log.info("Test importmasterkey rescan with pruning")

        proposer = self.nodes[2]
        normal_node = self.nodes[3]
        pruned_node = self.nodes[4]

        self.setup_stake_coins(proposer)

        # generate a bunch of blocks to allow for pruning
        connect_nodes(proposer, 0)
        connect_nodes(proposer, 1)
        proposer.generate(300)
        sync_blocks(self.nodes)
        pruned_node.pruneblockchain(288)  # prune everything but the minimum

        result = normal_node.importmasterkey(self._seed, self._passphrase, True)
        assert_equal(result['success'], True)

        assert_raises_rpc_error(-4, "Rescan is disabled in pruned mode", pruned_node.importmasterkey, self._seed,
                                self._passphrase, True)

        result = pruned_node.importmasterkey(self._seed, self._passphrase, False)
        assert_equal(result['success'], True)

        # Import now some mnemonics with funds in the genesis block to check that this is no rescanned
        normal_node.importmasterkey(regtest_mnemonics[0]['mnemonics'], "", True)
        pruned_node.importmasterkey(regtest_mnemonics[0]['mnemonics'], "", False)

        assert_equal(normal_node.getbalance(), 10000)
        assert_equal(pruned_node.getbalance(), 0)

    def run_test (self):
        self.test_import_consistency()
        self.test_rescan_with_pruning()

if __name__ == '__main__':
    WalletImportmasterkeyTest().main()
