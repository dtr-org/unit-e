#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Blockchain's rewards & UTXOs balance consistency"""

from random import (
    random,
    seed,
    setstate as rnd_setstate,
    getstate as rnd_getstate
)

from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import UnitETestFramework
from test_framework.util import sync_blocks, assert_equal


flt2dec = Decimal.from_float


class BlockchainBalanceTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [
            [],
            [],
            []
        ]

    def setup_chain(self):
        # The test starts with a pre-constructed chain
        super().setup_chain()

    def setup_network(self):
        # The nodes are connected as a chain by default, in order to have to
        # ability to easily split the network
        super().setup_network()

    def run_test(self):
        nodes = self.nodes

        # We import the wallet master keys so the nodes will be able to use some
        # funds as a stake to propose blocks (and to make transactions as well)
        self.__load_wallets(nodes)

        # We start our tests with an initial amount of created money
        created_money = nodes[0].gettxoutsetinfo()['total_amount']

        # We'll create a certain amount of money on the next text, so we update
        created_money = self.__test_empty_blocks_balance(created_money, nodes)
        self.__test_transaction_blocks_balance(created_money, nodes)

    def __test_empty_blocks_balance(self, created_money, nodes):
        # First: Checking without transactions, votes nor slashing
        for i in range(300):
            node_idx = i % 3

            self.__generate_block(nodes, node_idx)
            block_info = self.__get_last_block_info(node_idx, nodes)

            coinstake_tx_id = block_info['tx'][0]
            coinstake_tx_info = nodes[node_idx].gettransaction(coinstake_tx_id)

            # This assertion is very simplistic (it does not consider fees!)
            created_money += coinstake_tx_info['details'][0]['amount']
            assert_equal(
                created_money,
                nodes[node_idx].gettxoutsetinfo()['total_amount']
            )

        return created_money

    def __test_transaction_blocks_balance(self, created_money, nodes):
        # We'll use theses addresses to generate transactions
        node0_address = nodes[0].getaccountaddress('')
        node1_address = nodes[1].getaccountaddress('')
        node2_address = nodes[2].getaccountaddress('')
        rnd_state = rnd_getstate()
        seed(3141592)
        # Second: Checking with transactions
        for i in range(300):
            node_idx = i % 3

            self.__execute_random_transactions(
                node0_address, node1_address, node2_address, nodes
            )

            self.__generate_block(nodes, node_idx)
            block_info = self.__get_last_block_info(node_idx, nodes)

            block_tx = block_info['tx']
            coinstake_tx_id = block_tx[0]
            coinstake_tx_info = nodes[node_idx].gettransaction(coinstake_tx_id)

            created_money += coinstake_tx_info['details'][0]['amount']

            # This should fail now
            assert_equal(
                created_money,
                nodes[node_idx].gettxoutsetinfo()['total_amount']
            )
        rnd_setstate(rnd_state)

    def __get_last_block_info(self, node_idx, nodes):
        best_block_hash = nodes[node_idx].getbestblockhash()
        return nodes[node_idx].getblock(best_block_hash)

    def __execute_random_transactions(
            self, node0_address, node1_address, node2_address, nodes
    ):
        # The transactions themselves are not very important, but we want to
        # ensure that is hard to manually adjust the test to make it pass

        nodes[0].settxfee(flt2dec(0.0001))  # + 0.001 * random()))
        nodes[1].settxfee(flt2dec(0.0001))  # + 0.001 * random()))
        nodes[2].settxfee(flt2dec(0.0001))  # + 0.001 * random()))

        nodes[0].sendtoaddress(node1_address, flt2dec(1.0 + random()))
        nodes[0].sendtoaddress(node2_address, flt2dec(1.0 + random()))
        nodes[1].sendtoaddress(node0_address, flt2dec(1.0 + random()))
        nodes[1].sendtoaddress(node2_address, flt2dec(1.0 + random()))
        nodes[2].sendtoaddress(node0_address, flt2dec(1.0 + random()))
        nodes[2].sendtoaddress(node1_address, flt2dec(1.0 + random()))

    def __load_wallets(self, nodes):
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
    def __generate_block(nodes, node_idx):
        # It is rare but possible that a block was valid at the moment of
        # creation but invalid at submission. This is to account for those cases
        for i in range(5):
            try:
                nodes[node_idx].generate(1)
                sync_blocks(nodes)
                return
            except JSONRPCException as exp:
                print("error generating block:", exp.error)
        raise AssertionError("Node %s cannot generate block" % node_idx)


if __name__ == '__main__':
    BlockchainBalanceTest().main()
