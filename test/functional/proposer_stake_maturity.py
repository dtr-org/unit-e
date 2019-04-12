#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import (
    assert_equal,
    connect_nodes_bi,
    sync_blocks,
    wait_until,
)
from test_framework.test_framework import UnitETestFramework


class ProposerStakeMaturityTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [[
            '-minimumchainwork=0',
            '-maxtipage=1000000000'
        ]] * self.num_nodes
        self.setup_clean_chain = True
        self.customchainparams = [{"stake_maturity": 2}] * 2

    def run_test(self):
        nodes = self.nodes

        def has_synced_blockchain(i):
            status = nodes[i].proposerstatus()
            return status['wallets'][0]['status'] != 'NOT_PROPOSING_SYNCING_BLOCKCHAIN'

        self.log.info("Waiting for nodes to have started up...")
        wait_until(lambda: all(has_synced_blockchain(i)
                               for i in range(0, self.num_nodes)), timeout=5)

        self.log.info("Connecting nodes")
        connect_nodes_bi(nodes, 0, 1)

        def wait_until_all_have_reached_state(expected, which_nodes):
            def predicate(i):
                status = nodes[i].proposerstatus()
                return status['wallets'][0]['status'] == expected
            wait_until(lambda: all(predicate(i)
                                   for i in which_nodes), timeout=5)
            return predicate

        # none of the nodes has any money now, but a bunch of friends
        for i in range(self.num_nodes):
            status = nodes[i].proposerstatus()
            assert_equal(status['incoming_connections'], self.num_nodes - 1)
            assert_equal(status['outgoing_connections'], self.num_nodes - 1)

        self.setup_stake_coins(*self.nodes)

        # Generate stakeable outputs on both nodes
        nodes[0].generatetoaddress(1, nodes[0].getnewaddress('', 'bech32'))
        sync_blocks(nodes)
        nodes[1].generatetoaddress(1, nodes[1].getnewaddress('', 'bech32'))
        sync_blocks(nodes)

        # Current chain length doesn't overcome stake threshold, so
        # stakeable_balance == balance
        for i in range(self.num_nodes):
            self.check_node_balance(nodes[i], 10000, 10000)

        # Generate another block to overcome the stake threshold
        nodes[0].generatetoaddress(1, nodes[0].getnewaddress('', 'bech32'))
        sync_blocks(nodes)

        # Maturity check in action and we have one immature stake output
        self.check_node_balance(nodes[0], 10000, 9000)

        # Let's go further and generate yet another block
        nodes[0].generatetoaddress(1, nodes[0].getnewaddress('', 'bech32'))
        sync_blocks(nodes)

        # Now we have two immature stake outputs
        self.check_node_balance(nodes[0], 10000, 8000)

        # Generate two more blocks at another node
        nodes[1].generatetoaddress(2, nodes[1].getnewaddress('', 'bech32'))
        sync_blocks(nodes)

        # Thus all stake ouputs of the first node are mature
        self.check_node_balance(nodes[0], 10000, 10000)

        # Second node still have two immature stake outputs
        self.check_node_balance(nodes[1], 10000, 8000)

    def check_node_balance(self, node, balance, stakeable_balance):
        status = node.proposerstatus()
        wallet = status['wallets'][0]
        assert_equal(wallet['balance'], balance)
        assert_equal(wallet['stakeable_balance'], stakeable_balance)


if __name__ == '__main__':
    ProposerStakeMaturityTest().main()