#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
SnapshotWalletTest checks that node after syncing with the snapshot
can discover its transactions
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    sync_blocks,
    wait_until,
)
from test_framework.regtest_mnemonics import regtest_mnemonics


class SnapshotWalletTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True

        self.num_nodes = 3
        self.extra_args = [
            [],
            [],
            ['-prune=1', '-isd=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def has_finalized_snapshot(node, height):
            res = node.getblocksnapshot(node.getblockhash(height))
            if not res['valid']:
                return False
            return res['snapshot_finalized'] is True

        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node1_isd = self.nodes[2]

        connect_nodes(node0, node1.index)

        node0.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        node1.importmasterkey(regtest_mnemonics[1]['mnemonics'])
        assert_equal(node1.getbalance(), 10000)

        # leave IBD
        node0.generatetoaddress(1, node0.getnewaddress('', 'bech32'))
        sync_blocks([node0, node1], timeout=10)

        # transfer 1 satoshi to node1 to a1 address
        # F
        # e0 - e1[1, 2, 3, 4, 5]
        #            t1
        #            |> a1[+1]

        a1 = node1.getnewaddress('', 'bech32')
        t1 = node0.sendtoaddress(a1, 1)
        node0.generatetoaddress(4, node0.getnewaddress('', 'bech32'))
        assert_equal(node0.getblockcount(), 5)
        sync_blocks([node0, node1], timeout=10)
        assert_equal(node1.getbalance(), 10001)

        # transfer 2 satoshi to node1 to a2 address
        # transfer 6 satoshi to node1 to a1 address
        # F    J
        # e0 - e1[1, 2, ...] - e2[6, ...]
        #            t1           t2
        #            |> a1[+1]    |> a2[+2]
        #                         |> a1[+6]
        a2 = node1.getnewaddress('', 'bech32')
        tx = node0.createrawtransaction([], {a2: 2, a1: 6})
        tx_raw = node0.fundrawtransaction(tx)['hex']
        tx_raw = node0.signrawtransaction(tx_raw)['hex']
        t2 = node0.sendrawtransaction(tx_raw, True)
        node0.generatetoaddress(5, node0.getnewaddress('', 'bech32'))
        assert_equal(node0.getblockcount(), 10)
        sync_blocks([node0, node1], timeout=10)
        assert_equal(node1.getbalance(), 10009)

        # create finalized snapshot of epoch=2
        # F    F               F            J
        # e0 - e1[1, 2, ...] - e2[6, ...] - e3 - e4[16]
        #            t1           t2
        #            |> a1[+1]    |> a2[+2]
        #                         |> a1[+6]
        node0.generatetoaddress(6, node0.getnewaddress('', 'bech32'))
        assert_equal(node0.getblockcount(), 16)
        wait_until(lambda: has_finalized_snapshot(node0, height=9), timeout=10)

        # setup node1_isd before fast sync
        # we need to pre-generate 3 addresses that we used
        node1_isd.importmasterkey(regtest_mnemonics[1]['mnemonics'], '', False)
        node1_isd.getnewaddress('', 'bech32')  # address that was used for genesis output
        assert_equal(node1_isd.getnewaddress('', 'bech32'), a1)
        assert_equal(node1_isd.getnewaddress('', 'bech32'), a2)
        assert_equal(node1_isd.getbalance(), 0)

        # fast sync node1_isd with node0
        connect_nodes(node1_isd, node0.index)
        sync_blocks([node1_isd, node0], timeout=10)
        res = node1_isd.getblockchaininfo()
        assert_equal(res['pruned'], True)
        assert_equal(res['pruneheight'], 10)

        for n in [node1_isd, node1]:
            assert_equal(n.getbalance(), 10009)
            assert_equal(n.getwalletinfo()['txcount'], 3)


if __name__ == '__main__':
    SnapshotWalletTest().main()
