#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework, PROPOSER_REWARD
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    wait_until,
    sync_blocks,
)


CB_DEFAULT_OUTPUTS = 3


class FinalizationRewardsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config, '-validating=1'],
            [esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        node = self.nodes[0]
        finalizer1, finalizer2 = self.nodes[1:]

        self.setup_stake_coins(*self.nodes)

        # initial setup
        state = node.getfinalizationstate()
        assert_equal(state['currentEpoch'], 0)
        assert_equal(state['currentDynasty'], 0)
        assert_equal(state['lastFinalizedEpoch'], 0)
        assert_equal(state['lastJustifiedEpoch'], 0)
        assert_equal(state['validators'], 0)

        # leave IBD
        connect_nodes(node, finalizer1.index)
        connect_nodes(node, finalizer2.index)

        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        sync_blocks(self.nodes)

        payto = finalizer1.getnewaddress('', 'legacy')
        txid = finalizer1.deposit(payto, 10000)
        wait_until(lambda: txid in node.getrawmempool())
        txid = finalizer2.deposit(payto, 10000)
        wait_until(lambda: txid in node.getrawmempool())

        proposer_address1 = node.getnewaddress('', 'bech32')
        proposer_address2 = node.getnewaddress('', 'bech32')

        # 0 ... 4 ... 9 ... 14 ... 19 ... 24 ... 29 ... 34 .... 39 40
        #       F     F     F      F      F      F      J          tip
        node.generatetoaddress(27, proposer_address1)
        node.generatetoaddress(12, proposer_address2)

        # Check the reward output count in the first coinbase of an epoch
        block_hash = node.getbestblockhash()
        epoch_first_block = node.getblock(block_hash)
        epoch_first_cb = node.getrawtransaction(
            epoch_first_block['tx'][0], True, block_hash)
        assert_equal(len(epoch_first_cb['vout']), CB_DEFAULT_OUTPUTS + 5)

        addresses = sum((x['scriptPubKey']['addresses']
                         for x in epoch_first_cb[CB_DEFAULT_OUTPUTS:]), [])
        assert_equal(addresses[0:4], [proposer_address1] * 4)
        assert_equal(addresses[4:], [proposer_address2])

        # Check the reward output count in the last coinbase of an epoch
        block_hash = epoch_first_block['previousblockhash']
        epoch_last_block = node.getblock(block_hash)
        epoch_last_cb = node.getrawtransaction(
            epoch_last_block['tx'][0], True, block_hash)
        assert_equal(len(epoch_last_cb['vout']), CB_DEFAULT_OUTPUTS)

        # Check reward amount
        reward_amount = sum(
            x['value'] for x in epoch_first_cb['vout'][CB_DEFAULT_OUTPUTS:])
        assert_equal(reward_amount, PROPOSER_REWARD * 9 * 5)

        disconnect_nodes(node, finalizer2.index)

        # 0 .. 4 .. 9 .. 14 .. 19 .. 24 .. 29 .. 34 .. 39 .. 44 45
        #      F    F    F     F     F     F     J              tip
        node.generatetoaddress(5, node.getnewaddress('', 'bech32'))
        block_hash = node.getbestblockhash()
        epoch_first_block = node.getblock(block_hash)
        epoch_first_cb = node.getrawtransaction(
            epoch_first_block['tx'][0], True, block_hash)
        assert_equal(len(epoch_first_cb['vout']), CB_DEFAULT_OUTPUTS)

        connect_nodes(node, finalizer2.index)
        # 0 .. 4 .. 9 .. 14 .. 19 .. 24 .. 29 .. 34 .. 39 .. 44 .. 49 .. 54 45
        #      F    F    F     F     F     F     J            F     J      tip
        node.generatetoaddress(10, node.getnewaddress('', 'bech32'))

        # Check that, if the dynasty is several epochs long, we get the right
        # number of reward outputs
        block_hash = node.getbestblockhash()
        epoch_first_block = node.getblock(block_hash)
        epoch_first_cb = node.getrawtransaction(
            epoch_first_block['tx'][0], True, block_hash)
        assert_equal(len(epoch_first_cb['vout']), CB_DEFAULT_OUTPUTS + 15)


if __name__ == '__main__':
    FinalizationRewardsTest().main()
