#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (json, connect_nodes_bi, assert_equal,
                                 assert_raises_rpc_error)
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
        def __init__(self, framework, node, required_amounts):
            super().__init__()
            self.node = node
            self.address = node.getnewaddress("", "legacy")
            self.pubkey = node.validateaddress(self.address)["pubkey"]
            self.framework = framework

            # Workaround for a bug: When validator tx is rejected - funds it
            # used are locked for some time. Split the only 10k UTXO into
            # several by sending to self
            if len(required_amounts) == 0:
                return

            outputs = dict()
            for amount in required_amounts:
                outputs[node.getnewaddress()] = amount

            tx = node.sendmany("", outputs)
            framework.wait_for_transaction(tx, timeout=20)

        def deposit_reject(self):
            assert_raises_rpc_error(None, "Cannot create deposit",
                                    self.node.deposit, self.address,
                                    MIN_DEPOSIT_SIZE)

        def deposit_ok(self, deposit_amount=MIN_DEPOSIT_SIZE):
            tx = self.node.deposit(self.address, deposit_amount)
            self.framework.wait_for_transaction(tx, timeout=20)

        def logout_ok(self):
            tx = self.node.logout()
            self.framework.wait_for_transaction(tx, timeout=20)

        def withdraw_ok(self):
            tx = self.node.withdraw(self.address)
            self.framework.wait_for_transaction(tx, timeout=20)

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
                             '-whitelist=127.0.0.1',
                             '-esperanzaconfig=' + json_params]
        validator_settings = ['-proposing=0', '-validating=1', '-debug=all',
                              '-whitelist=127.0.0.1',
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

    def run_test(self):

        self.setup_stake_coins(*self.nodes)

        for node in self.nodes:
            assert_equal(node.initial_stake, node.getbalance())

        # Exit IBD
        self.generate_sync(self.nodes[0])

        # introduce actors
        proposer = self.nodes[0]
        admin = Admin.authorize(self, proposer)
        validator1 = AdminFullCycle.ValidatorWrapper(self, self.nodes[1],
                                                     [MIN_DEPOSIT_SIZE + 1] * 3)
        validator2 = AdminFullCycle.ValidatorWrapper(self, self.nodes[2],
                                                     [MIN_DEPOSIT_SIZE + 1] * 3)
        validator3 = AdminFullCycle.ValidatorWrapper(self, self.nodes[3], [])

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
        validator3.deposit_ok(MIN_DEPOSIT_SIZE * 2)

        # Finalize deposits
        self.generate_sync(proposer, EPOCH_LENGTH * 2)

        # Generate some blocks and check that validators are voting
        n_blocks = 2 * EPOCH_LENGTH
        self.generate_sync(proposer, n_blocks)

        assert (prev_n_blocks_have_txs_from(proposer, validator1.address,
                                            n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator2.address,
                                            n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator3.address,
                                            n_blocks))

        # Blacklist v1
        admin.blacklist([validator1.pubkey])
        self.generate_sync(proposer, n_blocks)

        assert (not prev_n_blocks_have_txs_from(proposer, validator1.address,
                                                n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator2.address,
                                            n_blocks))
        assert (prev_n_blocks_have_txs_from(proposer, validator3.address,
                                            n_blocks))

        # v1 should be able to logout even if blacklisted
        validator1.logout_ok()

        self.generate_sync(proposer, DYNASTY_LOGOUT_DELAY * EPOCH_LENGTH)
        self.generate_sync(proposer, WITHDRAWAL_EPOCH_DELAY * EPOCH_LENGTH)
        self.generate_sync(proposer, EPOCH_LENGTH)

        validator1.withdraw_ok()

        # End permissioning, v1 should be able to deposit again, v2 and v3
        # should continue voting
        admin.end_permissioning()

        validator1.deposit_ok()
        self.generate_sync(proposer, n_blocks)

        assert (
            prev_n_blocks_have_txs_from(proposer, validator1.address, n_blocks))
        assert (
            prev_n_blocks_have_txs_from(proposer, validator2.address, n_blocks))
        assert (
            prev_n_blocks_have_txs_from(proposer, validator3.address, n_blocks))


if __name__ == '__main__':
    AdminFullCycle().main()
