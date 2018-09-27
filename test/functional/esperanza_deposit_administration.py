#!/ usr / bin / env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import *
from test_framework.test_framework import UnitETestFramework
from test_framework.admin import *
import time


class DepositAdministration(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2

        params_data = {
            'epochLength': 10,
            'minDepositSize': 1500,
        }
        json_params = json.dumps(params_data)

        self.extra_args = [
            ['-proposing=1', '-debug=all', '-esperanzaconfig=' + json_params],
            ['-proposing=0', '-validating=1', '-debug=all',
             '-esperanzaconfig=' + json_params]]
        self.setup_clean_chain = True

    def assert_tx_rejected(self, txid):
        # UNIT-E: TODO: this is not reliable. Need a better way
        time.sleep(1)
        for i in range(0, self.num_nodes):
            try:
                self.nodes[i].getrawtransaction(txid)
            except:
                continue
            raise AssertionError("Transaction is found in node %s" % i)

    def run_test(self):
        proposer = self.nodes[0]
        validator = self.nodes[1]

        proposer.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        validator.importmasterkey('chef gas expect never jump rebel '
                                  'huge rabbit venue nature dwarf pact '
                                  'below surprise foam magnet science '
                                  'sister shrimp blanket example okay '
                                  'office ugly')

        assert_equal(10000, proposer.getbalance())
        assert_equal(10000, validator.getbalance())

        # Waiting for maturity
        proposer.generate(120)
        self.sync_all()

        admin = Admin.authorize(self, proposer)

        address = validator.getnewaddress("", "legacy")

        # At the moment validator has one UTXO with 10000 coins
        # Split it into two UTXO's
        exchange_tx = validator.sendtoaddress(address, 5000)
        self.wait_for_transaction(exchange_tx, timeout=10)

        # Validator is not in the whitelist and not allowed to deposit
        invalid_tx = validator.deposit(address, 2000)["transactionid"]
        self.assert_tx_rejected(invalid_tx)

        pubkey = validator.validateaddress(address)["pubkey"]

        command = {
            "cmd": "whitelist",
            "payload": [pubkey]
        }

        admin.send([command])

        deposit_tx = validator.deposit(address, 2000)["transactionid"]
        self.wait_for_transaction(deposit_tx, timeout=10)

        print("Test succeeded.")


if __name__ == '__main__':
    DepositAdministration().main()
