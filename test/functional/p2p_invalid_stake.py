#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet accounts properly when there is a double-spend conflict."""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal
from test_framework.mininode import (P2PInterface, network_thread_start)
from test_framework.messages import msg_block
from test_framework.blocktools import (get_tip_snapshot_meta, create_coinbase, sign_transaction, create_block)

class TestNode(P2PInterface):
    reject_messages = []
    def on_reject(self, message):
        self.reject_messages.append(message)

def get_staking_coin(node):
    unspent_outputs = node.listunspent()
    assert(len(unspent_outputs) > 0)
    return unspent_outputs[0]

def build_block_on_tip(node, staking_coin):
    tip = node.getblockheader(node.getbestblockhash())

    height = tip['height']
    snapshot_meta = get_tip_snapshot_meta(node)

    coinbase = create_coinbase(height + 1, staking_coin, snapshot_meta.hash)
    sign_transaction(node, coinbase)
    coinbase.rehash()
    block = create_block(int(tip['hash'], 16), coinbase, tip["mediantime"] + 1)
    block.solve()
    return block

class InvalidStakeTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-whitelist=127.0.0.1"]]

    def run_test(self):
        node = self.nodes[0]
        self.setup_stake_coins(node)

        p2p = node.add_p2p_connection(TestNode())
        network_thread_start()
        p2p.wait_for_verack()

        assert_equal(node.getblockcount(), 0)

        coin = get_staking_coin(node)

        # Check that already-spent coin is not accepted
        block = build_block_on_tip(node, coin)
        p2p.send_and_ping(msg_block(block))
        assert_equal(len(p2p.reject_messages), 0)
        assert_equal(node.getblockcount(), 1)

        block = build_block_on_tip(node, coin)
        p2p.send_and_ping(msg_block(block))

        assert_equal(len(p2p.reject_messages), 1)
        assert_equal(p2p.reject_messages[-1].message, b'block')
        assert_equal(p2p.reject_messages[-1].reason, b'bad-txns-inputs-missingorspent')

        assert_equal(node.getblockcount(), 1)


if __name__ == '__main__':
    InvalidStakeTest().main()

