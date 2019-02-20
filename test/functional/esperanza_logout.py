#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import JSONRPCException
from test_framework.util import wait_until
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.test_framework import UnitETestFramework
from test_framework.admin import Admin


class EsperanzaLogoutTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': 10,
            'dynastyLogoutDelay': 2,
            'withdrawalEpochDelay': 12
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-proposing=0',
            '-debug=all',
            '-rescan=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-proposing=0', '-debug=all', '-esperanzaconfig=' + json_params]

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

        deposit_tx = validator.deposit(payto, 10000)

        # wait for transaction to propagate
        self.wait_for_transaction(deposit_tx, 60)

        # mine 21 blocks (2 dynasties if we keep finalizing) to allow the deposit to get included in a block
        # and dynastyLogoutDelay to expire
        # +1 block to include a vote that was casted in 20th block
        for n in range(0, 21):
            self.generate_block(nodes[(n % 3) + 1])

        # ensure vote is created and included in the next block
        for n in self.nodes:
            wait_until(lambda: len(n.getrawmempool()) > 0, timeout=10)
        self.generate_block(nodes[1])
        self.sync_all()
        assert_equal(len(validator.getrawmempool()), 0)

        assert_equal(validator.getblockchaininfo()['blocks'], 142)

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        logout_tx = validator.logout()
        self.wait_for_transaction(logout_tx, 60)

        # wait for 2 dynasties since logout so we are not required to vote anymore
        for n in range(0, 20):
            self.generate_block(nodes[(n % 3) + 1])

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "NOT_VALIDATING")

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
    EsperanzaLogoutTest().main()
