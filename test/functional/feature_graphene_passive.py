#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests graphene by observing (and blocking if needed) p2p messages between two
normal nodes:
1) test_synced_mempool:
Tests graphene block when both nodes have exactly the same transactions in their
mempools
2) test_non_synced_mempool
Receiver and sender mempools are not synchronized. Checking that receiver still
can reconstruct block with help of GrapheneTx message and also checking that
receiver only asks for missing transactions
3) test_orphans:
Tests that receiver node looks not only on transactions in their mempool,
but also in the orphan pool
"""

from test_framework.mininode import (
    GrapheneTx,
    P2PInterface,
    network_thread_start,
    mininode_lock,
    msg_block,
    msg_tx,
    msg_sendcmpct,
    msg_graphenblock,
    msg_graphenetx,
    msg_cmpctblock,
)
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    sync_blocks,
    sync_mempools,
    wait_until,
    assert_equal
)
import time
from decimal import Decimal


class Graphene(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [['-debug=all',
                            '-whitelist=127.0.0.1',
                            '-stakesplitthreshold=2000000000']] * self.num_nodes
        self.setup_clean_chain = True

    def setup_network(self):
        super().setup_nodes()

    def run_test(self):
        relay = BlockingRelay(self.nodes[0], self.nodes[1], self.log)
        network_thread_start()
        relay.wait_for_verack()

        self.setup_stake_coins(self.nodes[0])

        # We don't allow high performance compact block in this test
        relay.allow((b'sendcmpct', False))

        # Exit IBD
        block_hash = self.nodes[0].generate(1)[0]
        relay.allow((b'block', block_hash))
        sync_blocks([self.nodes[0], self.nodes[1]])

        # self.test_synced_mempool(relay)
        # self.test_non_synced_mempool(relay)
        self.test_orphans(relay)

    def test_synced_mempool(self, relay):
        self.log.info("Testing synced mempool scenario")
        self.fill_mempool(relay)

        block_hash = self.nodes[0].generate(1)[0]
        relay.allow((b'graphenblock', block_hash))
        sync_blocks([self.nodes[0], self.nodes[1]])

    def test_non_synced_mempool(self, relay):
        self.log.info("Testing incomplete mempool scenario")
        self.fill_mempool(relay)

        # This tx won't be broadcast to node1, but it should instead get it
        # with graphentx
        missing_tx = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        block_hash = self.nodes[0].generate(1)[0]

        relay.allow((b'graphenblock', block_hash))
        relay.allow((b'graphenetx', block_hash))
        graphene_tx = relay.wait_for((b'graphenetx', block_hash)).graphene_tx

        # We want to make sure that only missing_tx was sent
        self.assert_graphene_txs(graphene_tx, [missing_tx])

        sync_blocks([self.nodes[0], self.nodes[1]])

    def test_orphans(self, relay):
        self.log.info("Testing orphan scenario")

        self.fill_mempool(relay)

        balance = Decimal(self.nodes[0].getbalance())
        balance_majority = balance * Decimal("0.51")
        half_balance = balance * Decimal("0.5")

        address = self.nodes[0].getnewaddress()
        
        # If we have an UTXO with 51% of our balance,
        # 50% balance UTXO can be created only as a child of it =>
        # we are 100% sure that child is indeed a child
        parent = self.nodes[0].sendtoaddress(address, balance_majority)
        child = self.nodes[0].sendtoaddress(address, half_balance)

        # We allow child, but not parent. Thus making child an orphan
        relay.allow((b'tx', child))
        relay.wait_for((b'tx', parent))
        relay.wait_for((b'tx', child))

        # Unfortunately we don't have a way to reliably sync orphan pool
        time.sleep(1)

        block_hash = self.nodes[0].generate(1)[0]
        relay.allow((b'graphenblock', block_hash))
        relay.allow((b'graphenetx', block_hash))
        graphene_tx = relay.wait_for((b'graphenetx', block_hash)).graphene_tx

        # We want to ensure that orphan tx was not sent, only the missing parent
        self.assert_graphene_txs(graphene_tx, [parent])

        sync_blocks([self.nodes[0], self.nodes[1]])

    def fill_mempool(self, relay):
        # We need to create lots of transactions to make graphene block
        # smaller than compact
        address = self.nodes[0].getnewaddress()

        # This value was picked by trial and error. If some parameters of
        # this test change - you might need to readjust it
        for _ in range(60):
            tx = self.nodes[0].sendtoaddress(address, 1)
            relay.allow((b'tx', tx))

        sync_mempools([self.nodes[0], self.nodes[1]])

    def assert_graphene_txs(self, graphene_tx: GrapheneTx, expected_hashes: []):
        assert_equal(len(expected_hashes), len(graphene_tx.txs))

        actual_hashes = []
        for tx in graphene_tx.txs:
            tx.rehash()
            actual_hashes.append(tx.hash)

        assert_equal(sorted(expected_hashes), sorted(actual_hashes))


def get_message_key(message):
    # Key is some identifier for a message AND it's content.
    # It might be different for different messages.
    # For example, for block it is ('block', block_hash)
    if isinstance(message, msg_block):
        block = message.block
        block.rehash()
        return message.command, block.hash

    if isinstance(message, msg_tx):
        tx = message.tx
        tx.rehash()
        return message.command, tx.hash

    if isinstance(message, msg_sendcmpct):
        return message.command, message.announce

    if isinstance(message, msg_graphenblock):
        header = message.block.header
        header.rehash()
        return message.command, header.hash

    if isinstance(message, msg_graphenetx):
        int_hash = message.graphene_tx.block_hash
        str_hash = "%064x" % int_hash
        return message.command, str_hash

    if isinstance(message, msg_cmpctblock):
        header = message.header_and_shortids.header
        header.rehash()
        return message.command, header.hash

    return None


# Connects 2 normal nodes through mininodes and acts as a p2p relay for all
# traffic between nodes. Can selectively block certain p2p messages
class BlockingRelay:
    class Node(P2PInterface):
        def __init__(self, name, log):
            super().__init__()
            self.send_to = None
            self.allowed_keys = set()
            self.log = log

            self.name = name
            self.seen_messages = dict()

            self.blocked_cache = dict()

        def on_inv(self, message):
            # This is implemented in P2PInterface, but we don't want it
            pass

        def on_data(self, command, raw_message):
            with mininode_lock:
                # Super call will decode raw message and put result to
                # self.last_message
                super().on_data(command, raw_message)
                deserialized_msg = self.last_message.get(
                    command.decode('ascii'))

                key = get_message_key(deserialized_msg)
                self.seen_messages[key] = deserialized_msg

                if key is not None:
                    self.handle_key(key, raw_message)
                    return

                self.send_to.send_data(command, raw_message)

        def handle_key(self, key, raw_message):
            if key in self.allowed_keys:
                self.log.info("%s: relayed %s" % (self.name, key))
                command = key[0]
                self.send_to.send_data(command, raw_message)
                self.blocked_cache.pop(key, None)
            else:
                if key not in self.blocked_cache:
                    self.log.info("%s: blocked %s" % (self.name, key))
                    self.blocked_cache[key] = raw_message

        def allow(self, key):
            with mininode_lock:
                self.allowed_keys.add(key)
                self._rehandle()

        def _rehandle(self):
            for (key, raw_message) in self.blocked_cache.copy().items():
                self.handle_key(key, raw_message)

    def __init__(self, node0, node1, log):
        super().__init__()
        self.mininode0 = BlockingRelay.Node("node%d" % node0.index, log)
        self.mininode1 = BlockingRelay.Node("node%d" % node1.index, log)

        self.mininode0.send_to = self.mininode1
        self.mininode1.send_to = self.mininode0

        node0.add_p2p_connection(self.mininode0)
        node1.add_p2p_connection(self.mininode1)

    def wait_for_verack(self):
        self.mininode0.wait_for_verack()
        self.mininode1.wait_for_verack()

    def allow(self, key):
        self.mininode0.allow(key)
        self.mininode1.allow(key)

    def wait_for(self, key):
        def get_msg():
            with mininode_lock:
                # Current test implementation does not rely on which node
                # received this message. We just want any
                msg = self.mininode0.seen_messages.get(key, None)
                if msg is not None:
                    return msg

                msg = self.mininode1.seen_messages.get(key, None)
                if msg is not None:
                    return msg

        wait_until(lambda: get_msg() is not None, timeout=10)

        return get_msg()


if __name__ == '__main__':
    Graphene().main()
