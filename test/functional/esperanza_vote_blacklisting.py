#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import *
from test_framework.test_framework import *
from test_framework.admin import *


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


# Checks that administrator can blacklist active validator and he can't vote
# anymore. Check is performed in two steps: first, ensure that validator is
# voting by checking for presence of his votes in blocks.
# Second, blacklist validator and check that there are no more votes from him
# in blocks
class VoteBlacklisting(UnitETestFramework):
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
        proposer.generate(COINBASE_MATURITY)
        self.sync_all()

        admin = Admin.authorize(self, proposer)

        address = validator.getnewaddress("", "legacy")
        pubkey = validator.validateaddress(address)["pubkey"]
        admin.whitelist([pubkey])

        deposit_tx = validator.deposit(address, 2000)["transactionid"]
        self.wait_for_transaction(deposit_tx, timeout=10)

        n_blocks = 20
        proposer.generate(n_blocks)
        self.sync_all()

        assert (prev_n_blocks_have_txs_from(proposer, address, n_blocks))

        admin.blacklist([pubkey])

        proposer.generate(n_blocks)
        self.sync_all()

        assert (not prev_n_blocks_have_txs_from(proposer, address, n_blocks))

        print("Test succeeded.")


if __name__ == '__main__':
    VoteBlacklisting().main()
