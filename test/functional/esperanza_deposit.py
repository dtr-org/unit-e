#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import JSONRPCException
from test_framework.regtest_mnemonics import regtest_mnemonics
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
            '-debug=all',
            '-rescan=1',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = [
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

        for i in range(self.num_nodes):
            nodes[i].importmasterkey(regtest_mnemonics[i]['mnemonics'])

        payto = validator.getnewaddress("", "legacy")

        assert_equal(validator.getbalance(), 10000)

        # wait for coinbase maturity
        for n in range(0, 119):
            self.generate_block(nodes[1])

        # generates 1 more block
        Admin.authorize_and_disable(self, nodes[1])

        txid = validator.deposit(payto, 10000)

        # wait for transaction to propagate
        self.wait_for_transaction(txid, 60)

        # mine some blocks to allow the deposit to get included in a block
        for n in range(0, 20):
            self.generate_block(nodes[(n % 3) + 1])

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "IS_VALIDATING")

    def generate_block(self, node):
        i = 0
        # It is rare but possible that a block was valid at the moment of creation but
        # invalid at submission. This is to account for those cases.
        while i < 5:
            try:
                self.generate_sync(node)
                return
            except JSONRPCException as exp:
                i += 1
                print("error generating block:", exp.error)
        raise AssertionError("Node" + str(node.index) + " cannot generate block")

if __name__ == '__main__':
    EsperanzaDepositTest().main()
