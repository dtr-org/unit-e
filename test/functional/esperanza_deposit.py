#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import sync_blocks
from test_framework.util import JSONRPCException
from test_framework.test_framework import UnitETestFramework
from test_framework.admin import Admin

class EsperanzaDepositTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': 10,
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-proposing=0',
            '-debug=all',
            '-rescan=1',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = [
            '-proposing=0',
            '-debug=all',
            '-txindex',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' + json_params
        ]

        self.extra_args = [validator_node_params,
                           proposer_node_params,
                           proposer_node_params,
                           proposer_node_params]
        self.setup_clean_chain = True

    def run_test(self):
        nodes = self.nodes

        validator = nodes[0]

        validator.importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        nodes[1].importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
        nodes[2].importmasterkey('narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat')
        nodes[3].importmasterkey('soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade')

        payto = validator.getnewaddress("", "legacy")

        assert_equal(validator.getbalance(), 10000)

        # wait for coinbase maturity
        for n in range(0, 119):
            self.generate_block(nodes[1])

        sync_blocks(self.nodes)

        # generates 1 more block
        Admin.authorize_and_disable(self, nodes[1])

        txid = validator.deposit(payto, 10000)

        # wait for transaction to propagate
        self.wait_for_transaction(txid)

        # mine some blocks to allow the deposit to get included in a block
        for n in range(0, 20):
            self.generate_block(nodes[(n % 3) + 1])

        sync_blocks(self.nodes)

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "IS_VALIDATING")

    def generate_block(self, node):
        i = 0
        # It is rare but possible that a block was valid at the moment of creation but
        # invalid at submission. This is to account for those cases.
        while i < 5:
            try:
                node.generate(1)
                return
            except JSONRPCException as exp:
                i += 1
                print("error generating block:", exp.error)
        raise AssertionError("Node" + str(node.index) + " cannot generate block")

if __name__ == '__main__':
    EsperanzaDepositTest().main()
