#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os

from test_framework.util import *
from test_framework.test_framework import *
from test_framework.admin import *
from esperanza_deposit_administration import assert_tx_rejected
from esperanza_vote_blacklisting import prev_n_blocks_have_txs_from


# Checks whole administration workflow, from whitelisgin/blacklisting to life
# after END_PERMISSIONING
class AdminFullCycle(UnitETestFramework):
    class ValidatorWrapper:
        def __init__(self, framework, node):
            super().__init__()
            self.node = node
            self.address = node.getnewaddress("", "legacy")
            self.pubkey = node.validateaddress(self.address)["pubkey"]
            self.framework = framework

            # split funds into several UTXOs
            exchange_tx = node.sendtoaddress(self.address, 7000)
            framework.wait_for_transaction(exchange_tx, timeout=10)
            exchange_tx = node.sendtoaddress(self.address, 4000)
            framework.wait_for_transaction(exchange_tx, timeout=10)

        def deposit_reject(self):
            tx = self.node.deposit(self.address, 2000)["transactionid"]
            assert_tx_rejected(self.node, tx)

        def deposit_ok(self):
            tx = self.node.deposit(self.address, 2000)["transactionid"]
            self.framework.wait_for_transaction(tx, timeout=10)

        def logout_ok(self):
            tx = self.node.logout()["transactionid"]
            self.framework.wait_for_transaction(tx, timeout=10)

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': 10,
            'minDepositSize': 1500,
            'dynastyLogoutDelay': 2,
            'withdrawalEpochDelay': 12
        }
        json_params = json.dumps(params_data)

        proposer_settings = ['-proposing=1', '-debug=all',
                             '-esperanzaconfig=' + json_params]
        validator_settings = ['-proposing=0', '-validating=1', '-debug=all',
                              '-esperanzaconfig=' + json_params]

        self.extra_args = [
            proposer_settings,
            validator_settings,
            validator_settings,
            validator_settings
        ]
        self.setup_clean_chain = True

    def run_test(self):
        proposer = self.nodes[0]

        self.nodes[0].importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')
        self.nodes[1].importmasterkey(
            'chef gas expect never jump rebel huge rabbit venue nature dwarf '
            'pact below surprise foam magnet science sister shrimp blanket '
            'example okay office ugly')
        self.nodes[2].importmasterkey(
            'narrow horror cheap tape language turn smart arch grow tired '
            'crazy squirrel sun pumpkin much panic scissors math pass tribe '
            'limb myself bone hat')
        self.nodes[3].importmasterkey(
            'soon empty next roof proof scorpion treat bar try noble denial '
            'army shoulder foam doctor right shiver reunion hub horror push '
            'theme language fade')

        for node in self.nodes:
            assert_equal(10000, node.getbalance())

        # Waiting for maturity
        proposer.generate(COINBASE_MATURITY)
        self.sync_all()

        # introduce actors
        proposer = self.nodes[0]
        admin = Admin.authorize(self, proposer)
        validator1 = AdminFullCycle.ValidatorWrapper(self, self.nodes[1])
        validator2 = AdminFullCycle.ValidatorWrapper(self, self.nodes[2])
        validator3 = AdminFullCycle.ValidatorWrapper(self, self.nodes[3])

        # No validators are whitelisted, v1, v2 will fail, v3 abstains
        validator1.deposit_reject()
        validator2.deposit_reject()

        # Whitelist v1, v1 succeeds, v2 fails, v3 abstains
        admin.whitelist([validator1.pubkey])
        validator1.deposit_ok()
        validator2.deposit_reject()

        n_blocks = 40
        proposer.generate(n_blocks)
        assert (
            prev_n_blocks_have_txs_from(proposer, validator1.address, n_blocks))
        assert (not prev_n_blocks_have_txs_from(proposer, validator2.address,
                                                n_blocks))
        assert (not prev_n_blocks_have_txs_from(proposer, validator3.address,
                                                n_blocks))

        # Blacklist v1, no votes allowed anymore
        admin.blacklist([validator1.pubkey])

        # print("attach")
        # input()
        # print("continue")
        proposer.generate(n_blocks)
        self.sync_all()

        assert (not prev_n_blocks_have_txs_from(proposer, validator1.address,
                                                n_blocks))
        assert (not prev_n_blocks_have_txs_from(proposer, validator2.address,
                                                n_blocks))
        assert (not prev_n_blocks_have_txs_from(proposer, validator3.address,
                                                n_blocks))

        # v1 should be able to logout even if blacklisted
        # UNIT-E: TODO: there is a bug preventing logout:
        # When one of the blacklisted votes is committed - it is first put into
        # the wallet and marked as spent, but only then admin validation happens
        # So at the moment of logout input is thought as spent, but actually not
        # validator1.logout_ok()

        # permissioning ended, all validators should be able to deposit
        admin.end_permissioning()

        # UNIT-E: TODO: add validator1 here after withdraw and bug above are
        # fixed
        validator2.deposit_ok()
        validator3.deposit_ok()

        proposer.generate(n_blocks)
        self.sync_all()

        # UNIT-E: TODO: the bug!
        # assert (
        #     prev_n_blocks_have_txs_from(proposer, validator1.address, n_blocks))
        assert (
            prev_n_blocks_have_txs_from(proposer, validator2.address, n_blocks))
        assert (
            prev_n_blocks_have_txs_from(proposer, validator3.address, n_blocks))

        print("Test succeeded.")


if __name__ == '__main__':
    AdminFullCycle().main()
