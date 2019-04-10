#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test node recovers its finalization state from disk
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    connect_nodes,
    disconnect_nodes,
    sync_blocks,
    wait_until,
)

def generate_block(proposer, count=1):
    proposer.generatetoaddress(count, proposer.getnewaddress('', 'bech32'))

def setup_deposit(self, proposer, validators):
    for _, n in enumerate(validators):
        n.new_address = n.getnewaddress("", "legacy")
        assert_equal(n.getbalance(), 10000)

    for n in validators:
        deptx = n.deposit(n.new_address, 1500)
        self.wait_for_transaction(deptx)

    generate_block(proposer, count=24)
    assert_equal(proposer.getblockcount(), 25)
    sync_blocks(validators + [proposer])
    for v in validators:
        disconnect_nodes(proposer, v.index)

class FinalizatoinStateRestoration(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}'],
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}', '-validating=1'],
        ]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()
        p, v = self.nodes
        connect_nodes(p, v.index)

    def restart_node(self, node):
        tip_before = node.getbestblockhash()
        super().restart_node(node.index)
        wait_until(lambda: node.getbestblockhash() == tip_before)

    def run_test(self):
        p, v = self.nodes
        self.setup_stake_coins(p, v)
        self.generate_sync(p)

        self.log.info("Setup deposit")
        setup_deposit(self, p, [v])

        self.log.info("Generate few epochs")
        self.generate_epoch(p, v, count=2)
        assert_equal(p.getblockcount(), 35)

        assert_finalizationstate(p, {'currentEpoch': 7,
                                     'lastJustifiedEpoch': 6,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        self.log.info("Restarting proposer")
        self.restart_node(p)

        # check it doesn't have peers -- i.e., loaded data from disk
        assert_equal(p.getpeerinfo(), [])

        self.log.info("Generate few epochs more")
        generate_block(p, count=9)
        assert_equal(p.getblockcount(), 44)

        # it is not connected to validator so that finalization shouldn't move
        assert_finalizationstate(p, {'currentEpoch': 9,
                                     'lastJustifiedEpoch': 6,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        # connect validator and chek how it votes
        self.wait_for_vote_and_disconnect(v, p)
        generate_block(p, count=1)

        assert_equal(p.getblockcount(), 45)
        assert_finalizationstate(p, {'currentEpoch': 9,
                                     'lastJustifiedEpoch': 8,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        self.generate_epoch(p, v, count=2)

        assert_equal(p.getblockcount(), 55)
        assert_finalizationstate(p, {'currentEpoch': 11,
                                     'lastJustifiedEpoch': 10,
                                     'lastFinalizedEpoch': 9,
                                     'validators': 1})

        self.log.info("Restarting validator")
        self.restart_node(v)

        # check it doesn't have peers -- i.e., loaded data from disk
        assert_equal(v.getpeerinfo(), [])

        self.log.info("Generate more epochs")
        self.generate_epoch(p, v, count=2)
        assert_equal(p.getblockcount(), 65)
        assert_finalizationstate(p, {'currentEpoch': 13,
                                     'lastJustifiedEpoch': 12,
                                     'lastFinalizedEpoch': 11,
                                     'validators': 1})

if __name__ == '__main__':
    FinalizatoinStateRestoration().main()
