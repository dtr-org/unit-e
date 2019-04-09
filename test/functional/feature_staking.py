#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import (
    Decimal
)
from test_framework.util import (
    Matcher,
    assert_equal,
    assert_matches,
)
from test_framework.test_framework import UnitETestFramework

class FeatureStakingTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0']]

    def run_test(self):

        node = self.nodes[0]

        self.log.info("Check that node does not have any stakeable coins before importing master key")
        result = node.getstakeablecoins()
        assert_equal(result['stakeable_coins'], [])

        self.log.info("Check that node does have one stakeable coin after importing master key")
        self.setup_stake_coins(node)
        result = node.getstakeablecoins()
        coins = result['stakeable_coins']
        assert_equal(len(coins), 1)
        assert_matches(coins, [
            {
                'coin': {
                    'amount': Matcher.eq(Decimal(10000)),
                    'script_pub_key': {
                        'type': 'witness_v0_keyhash',
                    }
                },
                'source_block': {
                    'height': 0,
                },
            },
        ], strict=False)

        address = coins[0]['coin']['script_pub_key']['addresses'][0]

        self.log.info("Generate a 100 staking blocks and check resulting chain of stakes")
        for height in range(100):
            result = node.tracechain(start=height, length=height + 1)
            previous_block_info = result['chain'][0]
            assert_equal(previous_block_info['block_height'], height)
            previous_block_txid = previous_block_info['coinbase' if height > 0 else 'initial_funds']['txid']
            previous_block_hash = previous_block_info['block_hash']

            result = node.generatetoaddress(1, address)
            assert_equal(len(result), 1)
            current_block_hash = result[0]
            result = node.tracestake(start=height + 1, length=height + 2, reverse=True)
            current_block_info = result[0]

            assert_equal(current_block_info['block_hash'], current_block_hash)
            assert_equal(current_block_info['status'], 'ondisk, stake found')
            current_block_stake_in = current_block_info['stake_txin']
            assert_equal(previous_block_hash, current_block_info['funding_block_hash'])
            assert_equal(current_block_stake_in['prevout']['txid'], previous_block_txid)

        self.log.info("Check that stakeable coins were properly re-used and spent")
        result = node.getstakeablecoins()
        coins = result['stakeable_coins']
        assert_equal(len(coins), 1)

if __name__ == '__main__':
    FeatureStakingTest().main()
