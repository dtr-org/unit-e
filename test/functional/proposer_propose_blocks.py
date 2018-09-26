#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework
import time

class EsperanzaTest(UnitETestFramework):

    def set_test_params(self):

        self.keys = [
            'swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready',
            'chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly',
            'narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat'
        ]

        self.num_nodes = len(self.keys)

        self.extra_args = list([
            '-proposing=1',
            '-minimumchainwork-0',
            '-maxtipage=1000000000'
        ] for i in range(0, self.num_nodes))
        self.setup_clean_chain = True

    def run_test(self):

        # import master keys which unlock funds from the genesis blocks
        for i in range(0, self.num_nodes):
            self.nodes[i].importmasterkey(self.keys[i])

        # connect all nodes with each other
        for i in range(0, self.num_nodes):
            for j in range(i+1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

        # wakes all the proposers in case they are sleeping right now
        for i in range(0, self.num_nodes):
            self.nodes[i].proposerwake()

        # now the funded nodes should have switched to IS_PROPOSING
        for i in range(0, self.num_nodes):
            status = self.nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('10000.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('10000.00000000'))
            assert_equal(wallet['status'], 'IS_PROPOSING')

        time.sleep(1)

        for i in range(0, self.num_nodes):
            print(self.nodes[i].proposerstatus())

        print("Test succeeded.")

if __name__ == '__main__':
    EsperanzaTest().main()
