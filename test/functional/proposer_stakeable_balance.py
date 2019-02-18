#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.test_framework import UnitETestFramework

class ProposerStakeableBalanceTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 6

        self.extra_args = list([
            '-proposing=1',
            '-minimumchainwork-0',
            '-maxtipage=1000000000'
        ] for i in range(0, self.num_nodes))
        self.setup_clean_chain = True

    def run_test(self):
        num_keys = 4
        nodes = self.nodes

        def has_synced_blockchain(i):
            def predicate():
                status = nodes[i].proposerstatus()
                return status['wallets'][0]['status'] != 'NOT_PROPOSING_SYNCING_BLOCKCHAIN'
            return predicate

        self.log.info("Waiting for nodes to have started up...")
        wait_until(lambda: all(has_synced_blockchain(i) for i in range(0, self.num_nodes)), timeout=5)

        self.log.info("Checking that no node is proposing as no node has a peer right now")
        for i in range(0, self.num_nodes):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('0.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('0.00000000'))
            assert_equal(wallet['status'], 'NOT_PROPOSING_NO_PEERS')

        self.log.info("Connecting nodes")
        for i in range(0, self.num_nodes):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(nodes, i, j)

        # wakes all the proposers in case they are sleeping right now
        for i in range(0, self.num_nodes):
            nodes[i].proposerwake()

        def has_reached_state(i, expected):
            def predicate():
                status = nodes[i].proposerstatus()
                return status['wallets'][0]['status'] == status
            return predicate

        self.log.info("Waiting for nodes to be connected (should read NOT_PROPOSING_NOT_ENOUGH_BALANCE then)")
        wait_until(lambda: all(has_reached_state(i, 'NOT_PROPOSING_NOT_ENOUGH_BALANCE') for i in range(0, self.num_nodes)), timeout=5)

        # none of the nodes has any money now, but a bunch of friends
        for i in range(0, self.num_nodes):
            status = nodes[i].proposerstatus()
            assert_equal(status['incoming_connections'], self.num_nodes - 1)
            assert_equal(status['outgoing_connections'], self.num_nodes - 1)

        self.log.info("Import master keys which should scan funds from genesis now")
        for i in range(num_keys):
            nodes[i].importmasterkey(regtest_mnemonics[i]['mnemonics'])

        # wakes all the proposers in case they are sleeping right now
        for i in range(self.num_nodes):
            nodes[i].proposerwake()

        self.log.info("The nodes with funds should advance to IS_PROPOSING")
        wait_until(lambda: all(has_reached_state(i, 'IS_PROPOSING') for i in range(0, num_keys)), timeout=5)

        self.log.info("The others sould stay in NOT_ENOUGH_BALANCE")
        wait_until(lambda: all(has_reached_state(i, 'NOT_PROPOSING_NOT_ENOUGH_BALANCE') for i in range(num_keys, self.num_nodes)), timeout=5)

        # now the funded nodes should have switched to IS_PROPOSING
        for i in range(0, num_keys):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('10000.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('10000.00000000'))
        # and others shoulds till not have enough funds for proposing blocks
        for i in range(num_keys, self.num_nodes):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('0.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('0.00000000'))
            assert_equal(wallet['status'], 'NOT_PROPOSING_NOT_ENOUGH_BALANCE')

        print("Test succeeded.")

if __name__ == '__main__':
    ProposerStakeableBalanceTest().main()
