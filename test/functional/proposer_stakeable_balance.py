#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from decimal import Decimal

from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    wait_until,
    disconnect_all_nodes,
    connect_nodes,
    sync_mempools
)
from test_framework.test_framework import UnitETestFramework


class ProposerStakeableBalanceTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 6
        self.setup_clean_chain = True
        self.extra_args = list([
            '-proposing=1',
            '-maxtipage=1000000000'
        ] for _ in range(0, self.num_nodes))

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        num_keys = 4
        nodes = self.nodes

        def has_synced_blockchain(i):
            status = nodes[i].proposerstatus()
            return status['wallets'][0]['status'] != 'NOT_PROPOSING_SYNCING_BLOCKCHAIN'

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

        def wait_until_all_have_reached_state(expected, which_nodes):
            def predicate(i):
                status = nodes[i].proposerstatus()
                return status['wallets'][0]['status'] == expected
            wait_until(lambda: all(predicate(i) for i in which_nodes), timeout=5)
            return predicate

        self.log.info("Waiting for nodes to be connected (should read NOT_PROPOSING_NOT_ENOUGH_BALANCE then)")
        wait_until_all_have_reached_state('NOT_PROPOSING_NOT_ENOUGH_BALANCE', range(0, self.num_nodes))

        # none of the nodes has any money now, but a bunch of friends
        for i in range(0, self.num_nodes):
            status = nodes[i].proposerstatus()
            assert_equal(status['incoming_connections'], self.num_nodes - 1)
            assert_equal(status['outgoing_connections'], self.num_nodes - 1)

        self.setup_stake_coins(*self.nodes[0:num_keys])

        # wakes all the proposers in case they are sleeping right now
        for i in range(self.num_nodes):
            nodes[i].proposerwake()

        self.log.info("The nodes with funds should advance to IS_PROPOSING")
        wait_until_all_have_reached_state('IS_PROPOSING', range(0, num_keys))

        self.log.info("The others should stay in NOT_ENOUGH_BALANCE")
        wait_until_all_have_reached_state('NOT_PROPOSING_NOT_ENOUGH_BALANCE', range(num_keys, self.num_nodes))

        # now the funded nodes should have switched to IS_PROPOSING
        for i in range(0, num_keys):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('10000.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('10000.00000000'))
        # and others should still not have enough funds for proposing blocks
        for i in range(num_keys, self.num_nodes):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('0.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('0.00000000'))

        # Check that if we send all the money to a P2PKH address we cannot stake anymore.
        disconnect_all_nodes(nodes)

        p2pkh_tx = nodes[0].sendtoaddress(nodes[0].getnewaddress("", "legacy"), nodes[0].getbalance(), "", "", True)

        wait_until(lambda: p2pkh_tx in nodes[0].getrawmempool())
        connect_nodes(nodes[0], nodes[1].index)
        sync_mempools(nodes[0:1])

        wait_until(lambda: nodes[0].gettransaction(p2pkh_tx)['confirmations'] > 0, timeout=60)

        assert_equal(len(nodes[0].liststakeablecoins()['stakeable_coins']), 0)
        assert_equal(nodes[0].liststakeablecoins()['stakeable_balance'], Decimal('0.00000000'))
        assert_equal(nodes[0].proposerstatus()['wallets'][0]['stakeable_balance'], Decimal('0.00000000'))

        print("Test succeeded.")

if __name__ == '__main__':
    ProposerStakeableBalanceTest().main()
