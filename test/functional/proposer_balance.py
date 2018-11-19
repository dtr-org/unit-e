#!/usr/bin/env python3

# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Blockchain's rewards & UTXOs balance consistency"""

from random import (
    getstate as rnd_getstate,
    randint,
    seed,
    setstate as rnd_setstate,
)

from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    sync_blocks,
    sync_mempools
)


class ProposerBalanceTest(UnitETestFramework):
    """
    This test checks that non-adversarial proposers keep consistent blockchain
    balances (i.e. the sum of the UTXO set amounts is equal to the accumulated
    inflation rewards)
    """

    def set_test_params(self):
        self.num_nodes = 3

    def run_test(self):
        nodes = self.nodes

        # We import the wallet master keys so the nodes will be able to use some
        # funds as a stake to propose blocks (and to make transactions as well)
        self.load_wallets(nodes)

        # We start our tests with an initial amount of created money
        created_money = nodes[0].gettxoutsetinfo()['total_amount']

        # First: Checking without transactions, votes nor slashing
        created_money = self.test_empty_blocks_balance(created_money, nodes)

        # Second: Checking with transactions & fees
        self.test_transaction_blocks_balance(created_money, nodes)

    def test_empty_blocks_balance(self, created_money, nodes):
        for i in range(15):
            node_idx = i % 3

            self.generate_block(nodes, node_idx)
            block_info = self.get_last_block_info(node_idx, nodes)

            coinstake_tx_id = block_info['tx'][0]
            coinstake_tx_info = nodes[node_idx].gettransaction(coinstake_tx_id)

            created_money += coinstake_tx_info['details'][0]['amount']
            assert_equal(
                created_money,
                nodes[node_idx].gettxoutsetinfo()['total_amount']
            )

        return created_money

    def test_transaction_blocks_balance(self, created_money, nodes):
        # We'll use theses addresses to generate transactions
        node0_address = nodes[0].getnewaddress()
        node1_address = nodes[1].getnewaddress()
        node2_address = nodes[2].getnewaddress()

        rnd_state = rnd_getstate()  # We do this to isolate this test
        seed(3141592)

        for i in range(12):
            node_idx = i % 3

            # We keep track of each transaction's "origin" in order to make
            # easier testing (gettransaction only works if the transaction was
            # created by the node's wallet).
            tx_map = self.execute_random_transactions(
                node0_address, node1_address, node2_address, nodes
            )

            self.generate_block(nodes, node_idx)
            block_info = self.get_last_block_info(node_idx, nodes)

            transactions = [
                # We fallback to node_idx for the 0th transaction
                nodes[tx_map.get(tx_id, node_idx)].gettransaction(tx_id)
                for tx_id in block_info['tx']
            ]

            coinstake_tx_info = transactions[0]
            created_money += coinstake_tx_info['details'][0]['amount']

            # We want to subtract the fees because are not created money
            for tx in transactions[1:]:
                created_money -= abs(tx['fee'])  # Fee is expressed as negative

            assert_equal(
                created_money,
                nodes[node_idx].gettxoutsetinfo()['total_amount']
            )

        rnd_setstate(rnd_state)

    def get_last_block_info(self, node_idx, nodes):
        best_block_hash = nodes[node_idx].getbestblockhash()
        return nodes[node_idx].getblock(best_block_hash)

    def execute_random_transactions(
            self, node0_address, node1_address, node2_address, nodes
    ):
        def random_fee():
            return Decimal(randint(10000, 19999)) / 100000000

        def random_amount():
            return Decimal(randint(100, 199)) / 100

        # The transactions themselves are not very important, but we want to
        # ensure that is hard to manually adjust the test to make it pass
        # WARNING: Decimal objects with "too much" precision will cause errors
        nodes[0].settxfee(random_fee())
        nodes[1].settxfee(random_fee())
        nodes[2].settxfee(random_fee())

        tx_ids = [
            nodes[0].sendtoaddress(node1_address, random_amount()),
            nodes[0].sendtoaddress(node2_address, random_amount()),
            nodes[1].sendtoaddress(node0_address, random_amount()),
            nodes[1].sendtoaddress(node2_address, random_amount()),
            nodes[2].sendtoaddress(node0_address, random_amount()),
            nodes[2].sendtoaddress(node1_address, random_amount()),
        ]

        # We ensure that these transactions will be included in the next block
        sync_mempools(nodes)

        # We return this mapping just for convenience, it's not really important
        return {
            tx_ids[0]: 0, tx_ids[1]: 0,
            tx_ids[2]: 1, tx_ids[3]: 1,
            tx_ids[4]: 2, tx_ids[5]: 2
        }

    def load_wallets(self, nodes):
        nodes[0].importmasterkey(
            'chef gas expect never jump rebel huge rabbit venue nature dwarf '
            'pact below surprise foam magnet science sister shrimp blanket '
            'example okay office ugly'
        )
        nodes[1].importmasterkey(
            'narrow horror cheap tape language turn smart arch grow tired '
            'crazy squirrel sun pumpkin much panic scissors math pass tribe '
            'limb myself bone hat'
        )
        nodes[2].importmasterkey(
            'soon empty next roof proof scorpion treat bar try noble denial '
            'army shoulder foam doctor right shiver reunion hub horror push '
            'theme language fade'
        )

    @staticmethod
    def generate_block(nodes, node_idx):
        try:
            nodes[node_idx].generatetoaddress(nblocks=1, address=nodes[node_idx].getnewaddress())
            sync_blocks(nodes)
        except JSONRPCException as exp:
            print("error generating block:", exp.error)
            raise AssertionError("Node %s cannot generate block" % node_idx)


if __name__ == '__main__':
    ProposerBalanceTest().main()
