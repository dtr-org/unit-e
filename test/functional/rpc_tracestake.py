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

class RpcTraceStakeTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0']]

    def run_test(self):
        node = self.nodes[0]
        self.setup_stake_coins(node)
        address = node.getnewaddress('', 'bech32')
        node.generatetoaddress(1, address)
        result = node.tracestake(start=1, length=2)
        assert_matches(result, [
            {
                'block_hash': Matcher.hexstr(64),
                'block_height': 0,
                'status': str,
            },
            {
                'block_hash': Matcher.hexstr(64),
                'block_height': 1,
                'funding_block_hash': Matcher.hexstr(64),
                'funding_block_height': 0,
                'stake_txout': {
                    'amount': Decimal,
                    'scriptPubKey': {
                        'asm': str,
                        'hex': Matcher.hexstr(44),
                        'reqSigs': int,
                        'type': str,
                        'addresses': Matcher.many(str, min=1)
                    }
                },
                'stake_txin': {
                    'prevout': {
                        'txid': Matcher.hexstr(64),
                        'n': Matcher.match(lambda x: x >= 0)
                    },
                    'scriptSig': {
                        'asm': str,
                        'hex': str
                    },
                    'scriptWitness': Matcher.many(str),
                },
                'status': str,
            },
        ])

if __name__ == '__main__':
    RpcTraceStakeTest().main()
