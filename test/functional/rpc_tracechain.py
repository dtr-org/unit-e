#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import (
    Decimal
)
from test_framework.util import (
    Matcher,
    assert_matches
)
from test_framework.test_framework import UnitETestFramework

class RpcTraceChainTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [['-stakesplitthreshold=0']]

    def run_test(self):
        node = self.nodes[0]
        self.setup_stake_coins(node)
        address = node.getnewaddress('', 'bech32')
        node.generatetoaddress(1, address)
        result = node.tracechain(start=1, length=2)
        script_pub_key_pattern = {
            'asm': str,
            'hex': Matcher.hexstr(44),
            'reqSigs': int,
            'type': str,
            'addresses': Matcher.many(str, min=1)
        }
        assert_matches(result, {
            'start_hash': Matcher.hexstr(64),
            'start_height': 1,
            'chain': [
                {
                    'block_hash': Matcher.hexstr(64),
                    'block_height': 1,
                    'stake_modifier': Matcher.hexstr(64),
                    'status': str,
                    'transactions': Matcher.many(Matcher.hexstr(64), min=1),
                    'coinbase': {
                        'txid': Matcher.hexstr(64),
                        'wtxid': Matcher.hexstr(64),
                        'stake': {
                            'prevout': {
                                'txid': Matcher.hexstr(64),
                                'n': Matcher.match(lambda x: x >= 0),
                            },
                            'scriptSig': {
                                'asm': str,
                                'hex': str,
                            },
                            'scriptWitness': Matcher.many(str),
                        },
                        'combined_stake': [],
                        'reward': {
                            'amount': Decimal,
                            'scriptPubKey': script_pub_key_pattern,
                        },
                        'returned_stake': Matcher.many({
                            'amount': Decimal,
                            'scriptPubKey': script_pub_key_pattern,
                        }, min=1),
                        'status': Matcher.many(str, min=1),
                    },
                },
                {
                    'block_hash': Matcher.hexstr(64),
                    'block_height': 0,
                    'stake_modifier': Matcher.hexstr(64),
                    'status': str,
                    'initial_funds': {
                        'txid': Matcher.hexstr(64),
                        'wtxid': Matcher.hexstr(64),
                        'amount': Decimal,
                        'length': int,
                        'outputs': Matcher.many({
                            'amount': Decimal,
                            'scriptPubKey': script_pub_key_pattern,
                        }),
                    },
                },
            ],
        })


if __name__ == '__main__':
    RpcTraceChainTest().main()
