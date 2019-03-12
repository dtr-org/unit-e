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
    sync_mempools,
    wait_until,
)

def generate_block(node):
    node.generatetoaddress(1, node.getnewaddress('', 'bech32'))

def setup_deposit(self, proposer, validators):

    for _, n in enumerate(validators):
        n.new_address = n.getnewaddress("", "legacy")
        assert_equal(n.getbalance(), 10000)

    for n in validators:
        deptx = n.deposit(n.new_address, 1500)
        self.wait_for_transaction(deptx)

    for _ in range(19):
        generate_block(proposer)

    assert_equal(proposer.getblockcount(), 20)

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

    def run_test(self):
        p, v = self.nodes
        self.setup_stake_coins(p, v)
        self.generate_sync(p)

        self.log.info("Setup deposit")
        setup_deposit(self, p, [v])

        self.log.info("Generate few epochs")
        for _ in range(16):
            generate_block(p)
        sync_blocks([p, v])
        sync_mempools([p, v])
        generate_block(p) # be sure vote is included
        sync_blocks([p, v])
        assert_equal(p.getblockcount(), 37)

        assert_finalizationstate(p, {'currentEpoch': 7,
                                     'lastJustifiedEpoch': 6,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        self.log.info("Restarting proposer")
        self.stop_node(p.index)
        self.start_node(p.index)

        # wait proposer operates
        wait_until(lambda: p.getblockcount() == 37, timeout=5)
        # check it doesn't have peers -- i.e., loaded data from disk
        assert_equal(p.getpeerinfo(), [])

        self.log.info("Generate few epochs more")
        for _ in range(10):
            generate_block(p)
        assert_equal(p.getblockcount(), 47)

        # it is not connected to validator so that finalization shouldn't move
        assert_finalizationstate(p, {'currentEpoch': 9,
                                     'lastJustifiedEpoch': 6,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        connect_nodes(p, v.index)
        sync_blocks([p, v])
        sync_mempools([p, v])
        generate_block(p) # be sure vote is included
        sync_blocks([p, v])
        assert_equal(p.getblockcount(), 48)

        assert_finalizationstate(p, {'currentEpoch': 9,
                                     'lastJustifiedEpoch': 8,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        for _ in range(9):
            generate_block(p)
        sync_blocks([p, v])
        sync_mempools([p, v])
        generate_block(p) # be sure vote is included
        sync_blocks([p, v])
        assert_equal(p.getblockcount(), 58)

        assert_finalizationstate(p, {'currentEpoch': 11,
                                     'lastJustifiedEpoch': 10,
                                     'lastFinalizedEpoch': 9,
                                     'validators': 1})

        self.log.info("Restaring validator")
        disconnect_nodes(p, v.index)
        self.stop_node(v.index)
        self.start_node(v.index)

        # wait validator operates
        wait_until(lambda: v.getblockcount() == 58, timeout=5)
        # check it doesn't have peers -- i.e., loaded data from disk
        assert_equal(v.getpeerinfo(), [])

        self.log.info("Generate more epochs")
        connect_nodes(p, v.index)
        for _ in range(59):
            generate_block(p)
        sync_blocks([p, v])
        sync_mempools([p, v])
        generate_block(p) # be sure vote is included
        sync_blocks([p, v])
        assert_equal(p.getblockcount(), 118)

        assert_finalizationstate(p, {'currentEpoch': 23,
                                     'lastJustifiedEpoch': 22,
                                     'lastFinalizedEpoch': 21,
                                     'validators': 1})


if __name__ == '__main__':
    FinalizatoinStateRestoration().main()
