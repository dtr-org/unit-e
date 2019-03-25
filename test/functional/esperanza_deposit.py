#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import (
    json,
    assert_equal,
    JSONRPCException,
    wait_until,
    assert_raises_rpc_error)
from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import UNIT


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

        self.setup_stake_coins(*self.nodes)
        assert_equal(validator.getbalance(), validator.initial_stake)

        # Leave IBD
        self.generate_sync(nodes[1])

        test_not_enough_money_for_deposit(validator)
        test_deposit_too_small(validator)
        self.test_successful_deposit(validator)
        test_duplicate_deposit(validator)

    def test_successful_deposit(self, validator):
        nodes = self.nodes

        payto = validator.getnewaddress("", "legacy")
        txid = validator.deposit(payto, 1500)

        deposit_tx = validator.gettransaction(txid)
        assert_equal(0, deposit_tx['amount'])  # we send the money to ourselves
        assert (deposit_tx['fee'] < 0)  # fee returned by gettransaction is negative

        raw_deposit_tx = validator.decoderawtransaction(deposit_tx['hex'])
        assert_equal(1500, raw_deposit_tx['vout'][0]['value'])
        assert_equal(10000 - 1500 + deposit_tx['fee'], raw_deposit_tx['vout'][1]['value'])

        # wait for transaction to propagate
        self.wait_for_transaction(txid, 60)

        wait_until(lambda: validator.getvalidatorinfo()['validator_status'] == 'WAITING_DEPOSIT_CONFIRMATION',
                   timeout=5)

        # mine a block to allow the deposit to get included
        self.generate_sync(nodes[2])

        wait_until(lambda: validator.getvalidatorinfo()['validator_status'] == 'WAITING_DEPOSIT_FINALIZATION',
                   timeout=5)

        # the validator will be ready to operate in epoch 4
        # TODO: UNIT - E: it can be 2 epochs as soon as #572 is fixed
        for n in range(0, 39):
            self.generate_block(nodes[(n % 3) + 1])

        wait_until(lambda: validator.getvalidatorinfo()['enabled'] == 1, timeout=5)
        assert_equal(validator.getvalidatorinfo()['validator_status'], 'IS_VALIDATING')

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


# Deposit all you got, not enough coins left for the fees
def test_not_enough_money_for_deposit(validator):
    payto = validator.getnewaddress("", "legacy")
    assert_raises_rpc_error(-25, "Cannot create deposit.", validator.deposit, payto, validator.initial_stake)


# Deposit less then the minimum
def test_deposit_too_small(validator):
    payto = validator.getnewaddress("", "legacy")
    assert_raises_rpc_error(-8, "Amount is below minimum allowed.", validator.deposit, payto, 100)


# Deposit again
def test_duplicate_deposit(validator):
    payto = validator.getnewaddress("", "legacy")
    assert_raises_rpc_error(-8, "The node is already validating.", validator.deposit, payto, 1500)


if __name__ == '__main__':
    EsperanzaDepositTest().main()
