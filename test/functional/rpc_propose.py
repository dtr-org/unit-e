#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test RPC calls propose and proposetoaddress

Tests correspond to code in proposer/proposer_rpc.cpp .
"""

from test_framework.test_framework import UnitETestFramework, DEFAULT_EPOCH_LENGTH
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    disconnect_nodes
)


class RpcProposeTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0'], ["-proposing=1"]]

    def setup_network(self):
        self.setup_nodes()
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        disconnect_nodes(node0, node1.index)
        disconnect_nodes(node1, node0.index)

    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        self.setup_stake_coins(node0, node1)

        assert_raises_rpc_error(-32603, "Node is automatically proposing.", node1.propose, 1)
        assert_raises_rpc_error(-32603, "Node is automatically proposing.",
                                node1.proposetoaddress, 1, node1.getnewaddress('', 'bech32'))

        assert_equal(len(node0.propose(10)), 10)
        assert_equal(node0.getblockcount(), 10)

        address = node0.getnewaddress('', 'bech32')

        proposed_blocks = node0.proposetoaddress(5, address)
        assert_equal(len(proposed_blocks), 5)
        assert_equal(node0.getblockcount(), 15)

        for i in proposed_blocks:
            block = node0.getblock(i)
            assert_equal(len(block['tx']), 1)
            details = node0.gettransaction(block['tx'][0])['details']
            # Might contain finalization rewards
            nb_of_rewards = DEFAULT_EPOCH_LENGTH + 1 if block['height'] % DEFAULT_EPOCH_LENGTH == 1 else 1
            assert_equal(len(details), nb_of_rewards)
            assert_equal(details[0]['address'], address)

        # Check that the script pubkey of the coin staked is also
        # used for the output
        assert_equal(len(node0.liststakeablecoins()['stakeable_coins']), 1)

        block_id = node0.propose(1)[0]
        coinbase_id = node0.getblock(block_id)['tx'][0]
        coinbase = node0.decoderawtransaction(node0.getrawtransaction(coinbase_id))
        coinbase_scriptpubkey = coinbase['vout'][0]['scriptPubKey']['hex']
        coin_scriptpubkey = node0.liststakeablecoins()['stakeable_coins'][0]['coin']['script_pub_key']['hex']
        assert_equal(coin_scriptpubkey, coinbase_scriptpubkey)


if __name__ == '__main__':
    RpcProposeTest().main()
