#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
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

        # wait for nodes to exit NOT_PROPOSING_SYNCING_BLOCKCHAIN status
        for i in range(0, self.num_nodes):
            wait_until(has_synced_blockchain(i))

        # at first none of the nodes will propose as it has no peers
        for i in range(0, self.num_nodes):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('0.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('0.00000000'))
            assert_equal(wallet['status'], 'NOT_PROPOSING_NO_PEERS')

        # connect all nodes with each other
        for i in range(0, self.num_nodes):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(nodes, i, j)

        # wakes all the proposers in case they are sleeping right now
        for i in range(0, self.num_nodes):
            nodes[i].proposerwake()

        # none of the nodes has any money now, but a bunch of friends
        for i in range(0, self.num_nodes):
            status = nodes[i].proposerstatus()
            assert_equal(status['incoming_connections'], self.num_nodes - 1)
            assert_equal(status['outgoing_connections'], self.num_nodes - 1)
            wallet = status['wallets'][0]
            assert_equal(wallet['status'], 'NOT_PROPOSING_NOT_ENOUGH_BALANCE')

        self.setup_stake_coins(*self.nodes[0:num_keys])

        # wakes all the proposers in case they are sleeping right now
        for i in range(self.num_nodes):
            nodes[i].proposerwake()

        # now the funded nodes should have switched to IS_PROPOSING
        for i in range(0, num_keys):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('10000.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('10000.00000000'))
            assert_equal(wallet['status'], 'IS_PROPOSING')
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
