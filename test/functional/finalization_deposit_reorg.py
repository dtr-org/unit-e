#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
DepositReorgTest checks how the state of finalizer
is progressing during re-orgs
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
    wait_until,
)
import os

ESPERANZA_CONFIG = '-esperanzaconfig={"epochLength": 10}'


class DepositReorgTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4

        self.extra_args = [
            [ESPERANZA_CONFIG],
            [ESPERANZA_CONFIG],
            [ESPERANZA_CONFIG],
            [ESPERANZA_CONFIG, '-validating=1'],
        ]
        self.setup_clean_chain = True

    def setup_network(self):
        # topology:
        # node[0] -> node[1]
        #         -> node[2]
        #         -> node[3]
        self.setup_nodes()
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)
        connect_nodes(self.nodes[0], 3)

    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node2 = self.nodes[2]
        finalizer = self.nodes[3]

        self.setup_stake_coins(node0, node1)

        # leave IBD
        # e0 - e1[1] node1, node2, node3, finalizer
        generate_block(node0)
        assert_equal(node0.getblockcount(), 1)
        sync_blocks([node0, node1, finalizer], timeout=10)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'NOT_VALIDATING')
        self.log.info('started epoch=1')

        # disconnect node1 to be able to revert finalizer's and node2's UTXOs later
        # e0 - e1[1] node0, node2, finalizer
        #         |
        #         - node1
        disconnect_nodes(node0, node1.index)

        # transfer amount to node2 which will be used to create an UTXO for finalizer
        #            t1
        # e0 - e1[1, 2] node0, node2, finalizer
        #         |
        #         - node1
        a1 = node2.getnewaddress('', 'bech32')
        t1 = node0.sendtoaddress(a1, 5000)
        assert t1 in node0.getrawmempool()
        generate_block(node0)
        assert_equal(node0.getblockcount(), 2)
        sync_blocks([node0, node2, finalizer], timeout=10)
        assert_equal(node2.getbalance(), 5000)
        self.log.info('created UTXO for node2')

        # receive amount for deposit
        #            t1 t2
        # e0 - e1[1, 2, 3] node0, node2, finalizer
        #         |
        #         - node1
        a2 = finalizer.getnewaddress('', 'bech32')
        t2 = node2.sendtoaddress(a2, 2000)
        assert t2 in node2.getrawmempool()
        wait_until(lambda: t2 in node0.getrawmempool(), timeout=150)
        generate_block(node0)
        sync_blocks([node0, node2, finalizer], timeout=10)
        assert_equal(node0.getblockcount(), 3)
        assert_equal(finalizer.getbalance(), 2000)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'NOT_VALIDATING')
        self.log.info('created UTXO for finalizer1')

        # disconnect node2 to be able to revert deposit
        #            t1 t2
        # e0 - e1[1, 2, 3] node0, finalizer
        #         |     |
        #         |     - node2
        #         - node1
        disconnect_nodes(node0, node2.index)

        # create deposit
        #            t1 t2 d1
        # e0 - e1[1, 2, 3, 4] node0, finalizer
        #         |     |
        #         |     - node2
        #         - node1
        a3 = finalizer.getnewaddress('', 'legacy')
        d1 = finalizer.deposit(a3, 1500)
        wait_until(lambda: d1 in node0.getrawmempool(), timeout=20)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_CONFIRMATION')
        self.log.info('validator_status is correct after creating deposit')

        generate_block(node0)
        assert_equal(node0.getblockcount(), 4)
        sync_blocks([node0, finalizer], timeout=10)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_FINALIZATION')
        self.log.info('validator_status is correct after mining deposit')

        # revert deposit
        #            t1 t2 d1
        # e0 - e1[1, 2, 3, 4] node0
        #         |     |
        #         |     -- 4, 5] node2, finalizer
        #         - node1
        generate_block(node2, count=2)
        assert_equal(node2.getblockcount(), 5)
        disconnect_nodes(node0, finalizer.index)
        connect_nodes(node2, finalizer.index)
        sync_blocks([node2, finalizer], timeout=10)
        assert_equal(finalizer.getblockcount(), 5)

        # we re-org'ed deposit but since the finalizer keeps
        # its txs in mempool, status must be WAITING_DEPOSIT_CONFIRMATION
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_CONFIRMATION')
        assert_equal(finalizer.gettransaction(d1)['txid'], d1)

        assert d1 in finalizer.resendwallettransactions()
        wait_until(lambda: d1 in finalizer.getrawmempool(), timeout=10)
        wait_until(lambda: d1 in node2.getrawmempool(), timeout=10)
        disconnect_nodes(node2, finalizer.index)
        self.log.info('validator_status is correct after reverting deposit')

        # revert UTXOs that lead to deposit
        #            t1 t2 d1
        # e0 - e1[1, 2, 3, 4] node0
        #         |     |
        #         |     -- 4, 5] node2
        #         -- 2, 3, 4, 5, 6] node1, finalizer
        generate_block(node1, count=5)
        assert_equal(node1.getblockcount(), 6)
        connect_nodes(node1, finalizer.index)
        sync_blocks([node1, finalizer], timeout=10)
        assert_equal(finalizer.getblockcount(), 6)

        # we re-org'ed all txs but since they are in mempool
        # validator_status shouldn't change
        wait_until(lambda: d1 in finalizer.getrawmempool(), timeout=10)
        wait_until(lambda: t2 in finalizer.getrawmempool(), timeout=10)
        wait_until(lambda: t1 in finalizer.getrawmempool(), timeout=10)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_CONFIRMATION')
        self.log.info('validator_status is correct after reverting all txs that led to deposit')

        # remove mempool.dat to simulate tx eviction
        # this keeps the same phase as wallet knows about its transactions
        # and expects to resend them once evicted txs are in the block
        self.stop_node(finalizer.index)
        os.remove(os.path.join(finalizer.datadir, "regtest", "mempool.dat"))
        self.start_node(finalizer.index, [ESPERANZA_CONFIG, '-validating=1'])
        wait_until(lambda: finalizer.getblockcount(), timeout=10)
        assert_equal(len(finalizer.getrawmempool()), 0)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_CONFIRMATION')
        self.log.info('validator_status is correct after removing mempool.dat')

        # add missing UTXOs and deposit to the node1 fork
        #            t1 t2 d1
        # e0 - e1[1, 2, 3, 4] node0
        #         |     |
        #         |     -- 4, 5] node2
        #         |                 t1 t2 d1
        #         -- 2, 3, 4, 5, 6, 7       ] node1, finalizer
        assert_equal(node1.sendrawtransaction(node0.getrawtransaction(t1)), t1)
        assert_equal(node1.sendrawtransaction(node0.getrawtransaction(t2)), t2)
        assert_equal(node1.sendrawtransaction(node0.getrawtransaction(d1)), d1)
        generate_block(node1)
        connect_nodes(node1, finalizer.index)
        sync_blocks([node1, finalizer], timeout=10)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_FINALIZATION')
        disconnect_nodes(node1, finalizer.index)
        self.log.info('validator_status is correct after re-mining txs on a different fork')

        # finalize deposit and re-org to that fork
        #            t1 t2 d1
        # e0 - e1[1, 2, 3, 4, ...           ] - ... - e4[31] node0, finalizer
        #         |     |
        #         |     -- 4, 5] node2
        #         |                 t1 t2 d1
        #         -- 2, 3, 4, 5, 6, 7       ] node1
        generate_block(node0, count=6)
        assert_equal(node0.getblockcount(), 10)
        assert_equal(node0.getfinalizationstate()['currentDynasty'], 0)
        for _ in range(2):
            generate_block(node0, count=10)
        assert_equal(node0.getblockcount(), 30)
        assert_equal(node0.getfinalizationstate()['currentDynasty'], 1)

        connect_nodes(node0, finalizer.index)
        sync_blocks([node0, finalizer], timeout=60)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_FINALIZATION')

        generate_block(node0)
        assert_equal(node0.getfinalizationstate()['currentDynasty'], 2)
        sync_blocks([node0, finalizer], timeout=10)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'IS_VALIDATING')
        self.log.info('validator_status is correct after re-organizing to the fork of finalized deposit')


if __name__ == '__main__':
    DepositReorgTest().main()
