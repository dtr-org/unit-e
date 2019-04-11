#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from binascii import unhexlify
from decimal import Decimal
from random import (
    getstate as rnd_getstate,
    randint,
    seed,
    setstate as rnd_setstate,
)
from time import time as time_time

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import (
    create_block,
    sign_coinbase,
    create_coinbase,
    create_tx_with_script,
    get_tip_snapshot_meta,
    get_finalization_rewards,
)
from test_framework.messages import (
    CTxOut,
    UNIT,
    msg_block,
    msg_headers,
    uint256_from_str
)
from test_framework.mininode import (
    P2PInterface
)
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.script import CScript
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    sync_blocks,
    sync_mempools,
    wait_until
)


class TestNode(P2PInterface):

    def __init__(self):
        super().__init__()
        self.reject_map = {}

    def on_reject(self, message):
        self.reject_map[message.data] = message.reason


class LTORTest(UnitETestFramework):
    """
    This test checks that LTOR (Lexicographic Transactions Ordering Rule)
    effectively is part of the consensus rules
    """

    def run_test(self):
        self.tip = None
        self.block_time = None

        # This mininode will help us to create blocks
        mininode = self.nodes[0].add_p2p_connection(TestNode())
        mininode.wait_for_verack()

        self.spendable_outputs = []
        self.load_wallets()

        rnd_state = rnd_getstate()  # We do this to isolate this test
        seed(2718281828459)

        self.exit_ibd_state()  # Exit IBD state, so we can sync mempools

        # run tests
        self.test_created_blocks_satisfy_ltor(sync_height=201)

        self.test_ltor_infringement_detection(sync_height=202)

        rnd_setstate(rnd_state)

    def test_ltor_infringement_detection(self, sync_height):
        txns = self.create_chained_transactions()
        block = self.get_empty_block(sync_height=sync_height)
        # We ensure that the transactions are NOT sorted in the correct order
        block.vtx.extend(sorted(txns, key=lambda _tx: _tx.hash, reverse=True))

        block.compute_merkle_trees()
        block.solve()

        self.nodes[0].p2p.send_message(msg_block(block))
        wait_until(lambda: self.nodes[0].p2p.reject_map, timeout=5)
        assert_equal(self.nodes[0].p2p.reject_map[int(block.hash, 16)], b'bad-tx-ordering')

    def test_created_blocks_satisfy_ltor(self, sync_height):
        recipient_addresses = [
            self.nodes[0].getnewaddress(),
            self.nodes[1].getnewaddress()
        ]

        tx_ids = self.ask_node_to_create_n_transactions(
            node_idx=0, num_tx=10, recipient_addresses=recipient_addresses
        )
        # We expect from the node to pick txns in the mempool and include them
        # in the next block
        self.ask_node_to_generate_block(0, sync_height=sync_height)

        # Block transactions are ordered lexicographically (except coinbase)
        block_tx_ids = self.get_tip_transactions(0)
        assert sorted(tx_ids) == block_tx_ids[1:]

        # We also check node1 to ensure that block is properly relayed
        block_tx_ids = self.get_tip_transactions(1)
        assert sorted(tx_ids) == block_tx_ids[1:]

    # Boilerplate functions:
    # --------------------------------------------------------------------------

    def set_test_params(self):
        self.num_nodes = 2
        # Required to avoid having our mininode banned for misbehaving
        self.extra_args = [['-whitelist=127.0.0.1'], ['-whitelist=127.0.0.1']]

    def load_wallets(self):
        self.nodes[0].importmasterkey(regtest_mnemonics[0]['mnemonics'])
        self.nodes[1].importmasterkey(regtest_mnemonics[1]['mnemonics'])

    def exit_ibd_state(self):
        # We generate a block to exit IBD state
        self.create_spendable_outputs(sync_height=200)

    def ask_node_to_generate_block(self, node_idx, sync_height):
        try:
            sync_blocks(self.nodes, height=sync_height)
            self.nodes[node_idx].generatetoaddress(
                nblocks=1,
                address=self.nodes[node_idx].getnewaddress()
            )
            # This call is safe because there's one node up to date
            sync_blocks(self.nodes, height=sync_height + 1)
        except JSONRPCException as exp:
            print("error generating block:", exp.error)
            raise AssertionError("Node %s cannot generate block" % node_idx)

    def ask_node_to_create_n_transactions(self, node_idx, num_tx, recipient_addresses):
        tx_ids = []
        num_addresses = len(recipient_addresses)

        node = self.nodes[node_idx]
        node.settxfee(Decimal(20000) / 100000000)

        for _ in range(num_tx):
            tx_id = node.sendtoaddress(
                recipient_addresses[randint(0, num_addresses-1)],
                Decimal(randint(100, 199)) / 100
            )
            tx_ids.append(tx_id)

        # We ensure that these transactions will be included in the next block
        sync_mempools(self.nodes)

        return tx_ids

    def create_chained_transactions(self):
        # We create more transactions here (doing it through blocks creation is
        # problematic because blocks are created too fast.
        last_tx = self.spendable_outputs[0].vtx[0]
        tx_value = int(0.95 * UNIT * 0.5)
        txns = [self.create_child_transaction(last_tx, tx_value, 0)]

        for divisor in map(lambda x: 2 ** x, range(2, 5)):
            tx_value = int(0.95 * UNIT / divisor)
            txns.extend([
                self.create_child_transaction(txns[-1], tx_value, 0),
                self.create_child_transaction(txns[-1], tx_value, 1)
            ])

        return txns

    def create_child_transaction(self, last_tx, tx_value, output_idx):
        tx = create_tx_with_script(last_tx, output_idx, b'', amount=tx_value)
        tx.vout.append(CTxOut(tx_value, CScript()))
        tx.rehash()
        return tx

    def get_tip_transactions(self, node_idx):
        node = self.nodes[node_idx]

        tip_hash = node.getbestblockhash()
        block_data = node.getblock(tip_hash)
        tx_hashes = block_data['tx']

        return tx_hashes

    def create_spendable_outputs(self, sync_height):
        block = self.get_empty_block(sync_height=sync_height)
        self.spendable_outputs.append(block)

        mininode = self.nodes[0].p2p
        mininode.send_message(msg_headers([block]))
        wait_until(self.is_block_hash_in_inv_predicate(block.hash))
        mininode.send_message(msg_block(block))
        sync_blocks(self.nodes, height=sync_height + 1)

    def is_block_hash_in_inv_predicate(self, block_hash):
        mininode = self.nodes[0].p2p
        hash_uint256 = uint256_from_str(unhexlify(block_hash)[::-1])

        def is_block_hash_in_inv():
            msg = mininode.last_message.get('getdata', None)
            if msg is None:
                return False
            return hash_uint256 in [cinv.hash for cinv in msg.inv]

        return is_block_hash_in_inv

    def get_empty_block(self, sync_height):
        sync_blocks(self.nodes, height=sync_height)
        node0 = self.nodes[0]

        hashprev = uint256_from_str(unhexlify(node0.getbestblockhash())[::-1])
        snapshot_hash = get_tip_snapshot_meta(node0).hash

        if len(self.spendable_outputs) > 0:
            block_time = self.spendable_outputs[-1].nTime + 1
            print(1, block_time)
        else:
            block_time = int(time_time()) + 2
            print(2, block_time)

        block = create_block(
            hashprev=hashprev,
            coinbase=sign_coinbase(node0, create_coinbase(
                height=sync_height + 1,
                stake=node0.listunspent()[0],
                snapshot_hash=snapshot_hash,
                finalization_rewards=get_finalization_rewards(node0)
            )),
            ntime=block_time
        )
        block.solve()

        return block


if __name__ == '__main__':
    LTORTest().main()
