#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.blocktools import (
    create_block,
    create_coinbase,
    get_tip_snapshot_meta,
    sign_coinbase,
)
from test_framework.util import (
    assert_equal,
    connect_nodes,
    sync_blocks,
    wait_until,
)
from test_framework.mininode import (
    P2PInterface,
    msg_witness_block,
    network_thread_start,
)
from test_framework.test_framework import UnitETestFramework


class P2P(P2PInterface):
    def __init__(self):
        super().__init__()
        self.messages = []
        self.rejects = []

    def reset_messages(self):
        self.messages = []
        self.rejects = []

    def on_commits(self, msg):
        self.messages.append(msg)

    def on_reject(self, msg):
        self.rejects.append(msg)

    def has_reject(self, err, block):
        for r in self.rejects:
            if r.reason == err and r.data == block:
                return True
        return False


class ProposerStakeMaturityTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [[
            '-minimumchainwork=0',
            '-maxtipage=1000000000'
        ]] * self.num_nodes
        self.setup_clean_chain = True
        self.customchainparams = [{"stake_maturity_activation_height": 2}] * 2

    def run_test(self):
        def build_block_with_immature_stake(node):
            height = node.getblockcount()
            stakes = node.listunspent()
            # Take the latest, immature stake
            stake = sorted(
                stakes,
                key=lambda x: x['confirmations'],
                reverse=False)[0]
            snapshot_meta = get_tip_snapshot_meta(node)
            coinbase = sign_coinbase(
                node, create_coinbase(
                    height, stake, snapshot_meta.hash))

            tip = int(node.getbestblockhash(), 16)
            block_time = node.getblock(
                self.nodes[0].getbestblockhash())['time'] + 1
            block = create_block(tip, coinbase, block_time)

            block.solve()
            return block

        def has_synced_blockchain(i):
            status = nodes[i].proposerstatus()
            return status['wallets'][0]['status'] != 'NOT_PROPOSING_SYNCING_BLOCKCHAIN'

        def wait_until_all_have_reached_state(expected, which_nodes):
            def predicate(i):
                status = nodes[i].proposerstatus()
                return status['wallets'][0]['status'] == expected
            wait_until(lambda: all(predicate(i)
                                   for i in which_nodes), timeout=5)
            return predicate

        def assert_number_of_connections(node, incoming, outgoing):
            status = node.proposerstatus()
            assert_equal(status['incoming_connections'], incoming)
            assert_equal(status['outgoing_connections'], outgoing)

        def check_reject(node, err, block):
            wait_until(lambda: node.p2p.has_reject(err, block), timeout=5)

        nodes = self.nodes

        # Create P2P connections to the second node
        self.nodes[1].add_p2p_connection(P2P())
        network_thread_start()

        self.log.info("Waiting untill the P2P connection is fully up...")
        wait_until(lambda: self.nodes[1].p2p.got_verack(), timeout=10)

        self.log.info("Waiting for nodes to have started up...")
        wait_until(lambda: all(has_synced_blockchain(i)
                               for i in range(0, self.num_nodes)), timeout=5)

        self.log.info("Connecting nodes")
        connect_nodes(nodes[0], nodes[1].index)

        assert_number_of_connections(
            self.nodes[0],
            self.num_nodes - 1,
            self.num_nodes - 1)
        assert_number_of_connections(
            self.nodes[1],
            self.num_nodes,
            self.num_nodes - 1)

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

        # Try to send the block with immature stake
        block = build_block_with_immature_stake(self.nodes[1])
        self.nodes[1].p2p.send_message(msg_witness_block(block))
        check_reject(self.nodes[1], b'bad-stake-immature', block.sha256)

    def check_node_balance(self, node, balance, stakeable_balance):
        status = node.proposerstatus()
        wallet = status['wallets'][0]
        assert_equal(wallet['balance'], balance)
        assert_equal(wallet['stakeable_balance'], stakeable_balance)


if __name__ == '__main__':
    ProposerStakeMaturityTest().main()
