#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import math
from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import JSONRPCException
from test_framework.util import wait_until
from test_framework.test_framework import UnitETestFramework


class EsperanzaWithdrawTest(UnitETestFramework):

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
            '-debug=all',
            '-rescan=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-debug=all', '-esperanzaconfig=' + json_params]

        self.extra_args = [validator_node_params,
                           proposer_node_params,
                           proposer_node_params,
                           proposer_node_params]
        self.setup_clean_chain = True

    def run_test(self):
        nodes = self.nodes
        validator = nodes[0]

        self.setup_stake_coins(*self.nodes)

        validator_address = validator.getnewaddress("", "legacy")

        assert_equal(validator.getbalance(), 10000)

        # LEave IBD
        self.generate_block(nodes[1])

        deposit_tx = validator.deposit(validator_address, 1500)

        # wait for transaction to propagate
        self.wait_for_transaction(deposit_tx, 30)

        # the validator will be ready to operate in epoch 4 and start voting on that checkpoint at height 49
        # TODO: UNIT - E: it can be 2 epochs as soon as #572 is fixed
        for n in range(0, 49):
            self.generate_block(nodes[(n % 3) + 1])

        # ensure vote is created and included in the next block
        for n in self.nodes:
            wait_until(lambda: len(n.getrawmempool()) > 0, timeout=10)
        self.generate_block(nodes[1])
        self.sync_all()
        assert_equal(len(validator.getrawmempool()), 0)

        assert_equal(validator.getblockchaininfo()['blocks'], 51)

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        logout_tx = validator.logout()
        self.wait_for_transaction(logout_tx, 30)

        # wait for 2 dynasties since logout so we are not required to vote anymore
        for n in range(0, 20):
            self.generate_block(nodes[(n % 3) + 1])

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "NOT_VALIDATING")

        # let's wait 14 epochs before trying to withdraw (12 for the delay and 3 for the
        # fact that the endEpoch is startEpoch(endDynasty + 1) )
        for n in range(0, 150):
            self.generate_block(nodes[(n % 3) + 1])

        # check that there is no leftover vote in the mempool,
        # this might happen if votes are outdated but not pruned yet
        assert_equal(len(validator.getrawmempool()), 0)

        withdraw_id = validator.withdraw(validator_address)
        self.wait_for_transaction(withdraw_id, 30)

        # let's mine the withdraw
        self.generate_block(nodes[1])

        # This is the initial deposit - fees for deposit, logout and withdraw
        assert_equal(math.ceil(validator.getbalance()), 10000)

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
    EsperanzaWithdrawTest().main()
