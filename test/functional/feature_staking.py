#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import (
    Decimal
)
from test_framework.util import (
    assert_equal,
    assert_matches,
)
from test_framework.test_framework import UnitETestFramework

def all_zeroes(hash_as_sha256_string):
    return 0 == int(hash_as_sha256_string, 16)

class FeatureStakingTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0', '-stakecombinemaximum=100000000000000']]

    def run_test(self):

        node = self.nodes[0]

        self.log.info("Check that node does not have any stakeable coins before importing master key")
        result = node.liststakeablecoins()
        assert_equal(result['stakeable_coins'], [])

        self.log.info("Check that node does have one stakeable coin after importing master key")
        self.setup_stake_coins(node)
        result = node.liststakeablecoins()
        coins = result['stakeable_coins']
        assert_equal(len(coins), 1)
        assert_matches(coins, [
            {
                'coin': {
                    'amount': Decimal(10000),
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

            if height == 0:
                # stake modifier of genesis block is zero by definition, every other block should have one
                assert all_zeroes(previous_block_info['stake_modifier'])
            else:
                assert not all_zeroes(previous_block_info['stake_modifier'])

            result = node.proposetoaddress(1, address)
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
        result = node.liststakeablecoins()
        coins = result['stakeable_coins']
        assert_equal(1, len(coins))

        self.log.info("Mine another block, this should make the first block reward mature")
        node.proposetoaddress(1, address)
        result = node.liststakeablecoins()
        coins = result['stakeable_coins']
        assert_equal(2, len(coins))
        assert_equal(set(map(lambda x: x['coin']['amount'], coins)), {Decimal(10000), Decimal(3.75)})

        self.log.info("Check that chaindata is maintained across restarts")
        chain_height = node.getblockcount()

        def get_chaindata(height):
            size = height + 1
            return (node.tracechain(start=height, length=size),
                    node.tracestake(start=height, length=size, reverse=True))
        chain_data_before_restart = get_chaindata(chain_height)
        self.restart_node(0)
        assert_equal(chain_height, node.getblockcount())
        chain_data_after_restart = get_chaindata(chain_height)
        assert_equal(chain_data_before_restart, chain_data_after_restart)


if __name__ == '__main__':
    FeatureStakingTest().main()
