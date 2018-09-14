#!/usr/bin/env python3
# Copyright (c) 2014-2017 The unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework
import time

class EsperanzaTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 6

        self.extra_args = list([
            '-proposing=1',
            '-minimumchainwork-0',
            '-maxtipage=1000000000'
        ] for i in range(0, self.num_nodes))
        self.setup_clean_chain = True

    def run_test(self):

        keys = [
            'swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready',
            'chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly',
            'narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat',
            'soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade'
        ]

        nodes = self.nodes

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

        # let some time pass to have the proposers update their state form the new situation
        time.sleep(0.5)

        # none of the nodes has any money now, but a bunch of friends
        for i in range(0, self.num_nodes):
            status = nodes[i].proposerstatus()
            assert_equal(status['incoming_connections'], self.num_nodes - 1)
            assert_equal(status['outgoing_connections'], self.num_nodes - 1)
            wallet = status['wallets'][0]
            assert_equal(wallet['status'], 'NOT_PROPOSING_NOT_ENOUGH_BALANCE')

        # import master keys which unlock funds from the genesis blocks
        for i in range(0, len(keys)):
            nodes[i].importmasterkey(keys[i])

        # wakes all the proposers in case they are sleeping right now
        for i in range(0, self.num_nodes):
            nodes[i].proposerwake()

        # let some time pass to have the proposers update their state form the new situation
        time.sleep(0.5)

        # now the funded nodes should have switched to IS_PROPOSING
        for i in range(0, len(keys)):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('10000.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('10000.00000000'))
            assert_equal(wallet['status'], 'IS_PROPOSING')
        # and others shoulds till not have enough funds for proposing blocks
        for i in range(len(keys), self.num_nodes):
            status = nodes[i].proposerstatus()
            wallet = status['wallets'][0]
            assert_equal(wallet['balance'], Decimal('0.00000000'))
            assert_equal(wallet['stakeable_balance'], Decimal('0.00000000'))
            assert_equal(wallet['status'], 'NOT_PROPOSING_NOT_ENOUGH_BALANCE')

        print("Test succeeded.")

if __name__ == '__main__':
    EsperanzaTest().main()
