#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json, connect_nodes_bi, assert_equal, \
    assert_raises_rpc_error
from test_framework.test_framework import UnitETestFramework, COINBASE_MATURITY
from test_framework.admin import Admin

MIN_DEPOSIT_SIZE = 1000
WITHDRAWAL_EPOCH_DELAY = 5
DYNASTY_LOGOUT_DELAY = 2
EPOCH_LENGTH = 10


def block_contains_txs_from(block, address):
    for tx in block["tx"]:
        for out in tx["vout"]:
            script_pub_key = out["scriptPubKey"]
            addresses = script_pub_key.get("addresses", None)
            if addresses is None:
                continue

            if address in addresses:
                return True

    return False


def prev_n_blocks_have_txs_from(node, validator_address, n):
    height = node.getblockcount()
    for i in range(0, n):
        block_hash = node.getblockhash(height - i)
        block = node.getblock(block_hash, 2)
        if block_contains_txs_from(block, validator_address):
            return True

    return False


# Checks whole administration workflow, from whitelisting/blacklisting to life
# after END_PERMISSIONING
class AdminFullCycle(UnitETestFramework):
    class ValidatorWrapper:
        def __init__(self, framework, node, n_utxos_needed):
            super().__init__()
            self.node = node
            self.address = node.getnewaddress("", "legacy")
            self.pubkey = node.validateaddress(self.address)["pubkey"]
            self.framework = framework

            # Workaround for a bug: When validator tx is rejected - funds it
            # used are locked for some time. Split the only 10k UTXO into
            # several by sending to self
            max_utxo = node.getbalance()
            for i in range(n_utxos_needed - 1):
                max_utxo -= (MIN_DEPOSIT_SIZE + 1)
                exchange_tx = node.sendtoaddress(self.address, max_utxo)
                framework.wait_for_transaction(exchange_tx, timeout=10)

        def deposit_reject(self):
            assert_raises_rpc_error(None, "Cannot create deposit",
                                    self.node.deposit, self.address,
                                    MIN_DEPOSIT_SIZE)

        def deposit_ok(self):
            tx = self.node.deposit(self.address, MIN_DEPOSIT_SIZE)
            self.framework.wait_for_transaction(tx, timeout=10)

        def logout_ok(self):
            tx = self.node.logout()
            self.framework.wait_for_transaction(tx, timeout=10)

        def withdraw_ok(self):
            tx = self.node.withdraw(self.address)
            self.framework.wait_for_transaction(tx, timeout=10)

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': EPOCH_LENGTH,
            'minDepositSize': MIN_DEPOSIT_SIZE,
            'dynastyLogoutDelay': DYNASTY_LOGOUT_DELAY,
            'withdrawalEpochDelay': WITHDRAWAL_EPOCH_DELAY
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

    def setup_network(self):
        super().setup_network()

        # Fully connected graph works faster
        for i in range(self.num_nodes):
            for j in range(i + 1, self.num_nodes):
                connect_nodes_bi(self.nodes, i, j)

    def sync_generate(self, node, n_blocks):
        # When it comes to checking votes - it is important to always sync
        # all - this guarantees that votes are not outdated/stuck anywhere
        for _ in range(n_blocks):
            node.generate(1)
            self.sync_all()

    def run_test(self):
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
        self.nodes[0].generate(COINBASE_MATURITY)
        self.sync_all()

        # introduce actors
        proposer = self.nodes[0]
        admin = Admin.authorize(self, proposer)
        validator1 = AdminFullCycle.ValidatorWrapper(self, self.nodes[1], 3)
        validator2 = AdminFullCycle.ValidatorWrapper(self, self.nodes[2], 3)
        validator3 = AdminFullCycle.ValidatorWrapper(self, self.nodes[3], 1)

        # No validators are whitelisted any deposits should fail
        validator1.deposit_reject()
        validator2.deposit_reject()

        # Whitelist v1
        admin.whitelist([validator1.pubkey])
        validator1.deposit_ok()
        validator2.deposit_reject()  # ensure only v1 is whitelisted

        # Whitelist v2
        admin.whitelist([validator2.pubkey])
        validator2.deposit_ok()

        # Whitelist v3
        admin.whitelist([validator3.pubkey])
        validator3.deposit_ok()

        # Generate some blocks and check that validators are voting
        n_blocks = 2 * EPOCH_LENGTH
        self.sync_generate(proposer, n_blocks)

        assert (prev_n_blocks_have_txs_from(proposer, validator1.address,
                                            n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator2.address,
                                            n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator3.address,
                                            n_blocks))

        # Blacklist v1
        admin.blacklist([validator1.pubkey])
        self.sync_generate(proposer, n_blocks)

        assert (not prev_n_blocks_have_txs_from(proposer, validator1.address,
                                                n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator2.address,
                                            n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator3.address,
                                            n_blocks))

        # v1 should be able to logout even if blacklisted
        validator1.logout_ok()

        self.sync_generate(proposer, DYNASTY_LOGOUT_DELAY * EPOCH_LENGTH)
        self.sync_generate(proposer, WITHDRAWAL_EPOCH_DELAY * EPOCH_LENGTH)
        self.sync_generate(proposer, EPOCH_LENGTH)

        validator1.withdraw_ok()

        # End permissioning, v1 should be able to deposit again, v2 and v3
        # should continue voting
        admin.end_permissioning()

        validator1.deposit_ok()
        self.sync_generate(proposer, n_blocks)

        assert (
            prev_n_blocks_have_txs_from(proposer, validator1.address, n_blocks))
        assert (
            prev_n_blocks_have_txs_from(proposer, validator2.address, n_blocks))
        assert (
            prev_n_blocks_have_txs_from(proposer, validator3.address, n_blocks))

        print("Test succeeded.")


if __name__ == '__main__':
    AdminFullCycle().main()
