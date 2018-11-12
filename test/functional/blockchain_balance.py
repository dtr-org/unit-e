#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Blockchain's rewards & UTXOs balance consistency"""
from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.test_framework import UnitETestFramework
from test_framework.util import sync_blocks, assert_equal


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
        # funds as a stake to propose blocks
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

        created_money = Decimal('0')  # TODO: Fix the amount
        utxo_set = {}  # TODO: Fix the value

        for i in range(10):
            node_idx = i % 3
            self.generate_block(nodes[node_idx])

            sync_blocks(nodes)

            best_block_hash = nodes[node_idx].getbestblockhash()
            block_info = nodes[node_idx].getblock(best_block_hash)

            coinstake_tx_id = block_info['tx'][0]
            coinstake_tx_info = nodes[node_idx].gettransaction(coinstake_tx_id)

            created_money += coinstake_tx_info['details'][0]['amount']
            print()
            print(repr(created_money))
            print()

    @staticmethod
    def generate_block(node):
        # It is rare but possible that a block was valid at the moment of
        # creation but invalid at submission. This is to account for those cases
        for i in range(5):
            try:
                node.generate(1)
                return
            except JSONRPCException as exp:
                print("error generating block:", exp.error)
        raise AssertionError("Node %s cannot generate block" % node.index)


if __name__ == '__main__':
    BlockchainBalanceTest().main()
