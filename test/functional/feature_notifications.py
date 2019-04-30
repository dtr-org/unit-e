#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the -alertnotify, -blocknotify and -walletnotify options."""
import os

from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal, wait_until, connect_nodes_bi

class NotificationsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def setup_network(self):
        self.alertnotify_dir = os.path.join(self.options.tmpdir, "alertnotify")
        self.blocknotify_dir = os.path.join(self.options.tmpdir, "blocknotify")
        self.walletnotify_dir = os.path.join(self.options.tmpdir, "walletnotify")
        os.mkdir(self.alertnotify_dir)
        os.mkdir(self.blocknotify_dir)
        os.mkdir(self.walletnotify_dir)

        # -alertnotify and -blocknotify on node0, walletnotify on node1
        self.extra_args = [["-blockversion=2",
                            "-alertnotify=echo > {}".format(os.path.join(self.alertnotify_dir, '%s')),
                            "-blocknotify=echo > {}".format(os.path.join(self.blocknotify_dir, '%s'))],
                           ["-blockversion=211",
                            "-rescan",
                            "-walletnotify=echo > {}".format(os.path.join(self.walletnotify_dir, '%s'))]]
        super().setup_network()

    def run_test(self):
        self.setup_stake_coins(*self.nodes)

        self.log.info("test -blocknotify")
        block_count = 10
        blocks = self.nodes[1].generatetoaddress(block_count, self.nodes[1].getnewaddress('', 'bech32'))

        # wait at most 10 seconds for expected number of files before reading the content
        wait_until(lambda: len(os.listdir(self.blocknotify_dir)) == block_count, timeout=10)

        # directory content should equal the generated blocks hashes
        assert_equal(sorted(blocks), sorted(os.listdir(self.blocknotify_dir)))

        self.log.info("test -walletnotify")
        # wait at most 10 seconds for expected number of files before reading the content
        # The transaction count should include the one in the genesis block
        wait_until(lambda: len(os.listdir(self.walletnotify_dir)) == block_count + 1, timeout=10)

        # directory content should equal the generated transaction hashes
        txids_rpc = set(map(lambda t: t['txid'], self.nodes[1].listtransactions("*", 100)))
        assert_equal(set(os.listdir(self.walletnotify_dir)), txids_rpc)
        for tx_file in os.listdir(self.walletnotify_dir):
            os.remove(os.path.join(self.walletnotify_dir, tx_file))

        self.log.info("test -walletnotify after rescan")
        # restart node to rescan to force wallet notifications
        self.restart_node(1)
        connect_nodes_bi(self.nodes, 0, 1)

        wait_until(lambda: len(os.listdir(self.walletnotify_dir)) == block_count + 1, timeout=10)

        # directory content should equal the generated transaction hashes
        txids_rpc = set(map(lambda t: t['txid'], self.nodes[1].listtransactions("*", 100)))
        assert_equal(set(os.listdir(self.walletnotify_dir)), txids_rpc)

        # TODO: UNIT-E: re-enable or delete https://github.com/dtr-org/unit-e/issues/472
        # # Mine another 41 up-version blocks. -alertnotify should trigger on the 51st.
        # self.log.info("test -alertnotify")
        # self.nodes[1].generate(41)
        # assert_equal(self.nodes[1].getblockcount(), 51)
        # self.sync_all()
        #
        # # Give unit-e 10 seconds to write the alert notification
        # wait_until(lambda: len(os.listdir(self.alertnotify_dir)), timeout=10)
        #
        # for notify_file in os.listdir(self.alertnotify_dir):
        #     os.remove(os.path.join(self.alertnotify_dir, notify_file))
        #
        # # Mine more up-version blocks, should not get more alerts:
        # self.nodes[1].generate(2)
        # self.sync_all()
        #
        # self.log.info("-alertnotify should not continue notifying for more unknown version blocks")
        # assert_equal(len(os.listdir(self.alertnotify_dir)), 0)

if __name__ == '__main__':
    NotificationsTest().main()
