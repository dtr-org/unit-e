#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import (
    Decimal,
)
from test_framework.util import (
    Matcher,
    assert_matches,
)
from test_framework.test_framework import UnitETestFramework

class RpcGetStakeableBalanceTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0']]

    def run_test(self):
        node = self.nodes[0]
        self.setup_stake_coins(node)
        result = node.liststakeablecoins()
        assert_matches(result, {
            'stakeable_balance': Decimal,
            'stakeable_coins': [
                {
                    'coin': {
                        'amount': Decimal,
                        'script_pub_key': {
                            'asm': str,
                            'hex': Matcher.hexstr(44),
                            'reqSigs': int,
                            'type': str,
                            'addresses': Matcher.many(str, min=1)
                        },
                        'out_point': {
                            'txid': Matcher.hexstr(64),
                            'n': Matcher.match(lambda v: v >= 0),
                        },
                    },
                    'source_block': {
                        'height': int,
                        'hash': str,
                        'time': int,
                    },
                },
            ]
        })


if __name__ == '__main__':
    RpcGetStakeableBalanceTest().main()
