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
from time import sleep

from test_framework.authproxy import JSONRPCException
from test_framework.blocktools import (
    SnapshotMeta,
    create_block,
    create_coinbase,
    create_transaction
)
from test_framework.comptool import (
    RejectResult,
    TestInstance,
    TestManager
)
from test_framework.messages import (
    CTxOut,
    UNIT,
    uint256_from_str
)
from test_framework.mininode import network_thread_start
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.script import CScript
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    sync_blocks,
    sync_mempools
)


class LTORTest(UnitETestFramework):
    """
    This test checks that LTOR (Lexicographic Transactions Ordering Rule)
    effectively is part of the consensus rules
    """

    def run_test(self):
        test = TestManager(self, self.options.tmpdir)
        test.add_all_connections(self.nodes)
        self.tip = None
        self.block_time = None
        network_thread_start()
        test.run()

    def get_tests(self):
        self.spendable_outputs = []
        self.load_wallets()

        rnd_state = rnd_getstate()  # We do this to isolate this test
        seed(2718281828459)

        self.exit_ibd_state()  # Exit IBD state, so we can sync mempools

        for test_result in self.test_ltor_infringement_detection():
            yield test_result
        for test_result in self.test_created_blocks_satisfy_ltor():
            yield test_result

        rnd_setstate(rnd_state)

    def test_ltor_infringement_detection(self):
        # Just creating easily accessible outputs (coinbase) for next txns
        yield self.create_spendable_outputs()
        # We need to increase the seconds count at least by 1 before creating
        # the new block.
        sleep(1)

        txns = self.create_chained_transactions()
        block = self.get_empty_block()
        # We ensure that the transactions are NOT sorted in the correct order
        block.vtx.extend(sorted(txns, key=lambda _tx: _tx.hash, reverse=True))

        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()

        yield TestInstance(
            [[block, RejectResult(16, b'tx-ordering')]],
            test_name='test_ltor_infringement_detection'
        )

    def test_created_blocks_satisfy_ltor(self):
        recipient_addresses = [
            self.nodes[0].getnewaddress(),
            self.nodes[1].getnewaddress()
        ]

        tx_ids = self.ask_node_to_create_n_transactions(
            node_idx=0, num_tx=20, recipient_addresses=recipient_addresses
        )
        self.generate_block(0)

        # Block transactions are ordered lexicographically (except coinbase)
        block_tx_ids = self.get_tip_transactions(0)
        assert sorted(tx_ids) == block_tx_ids[1:]
        block_tx_ids = self.get_tip_transactions(1)
        assert sorted(tx_ids) == block_tx_ids[1:]

        yield TestInstance(test_name="test_created_blocks_satisfy_ltor")

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
        # We generate a couple of blocks to exit IBD state
        self.generate_block(0)
        self.generate_block(1)

    def generate_block(self, node_idx):
        try:
            self.nodes[node_idx].generatetoaddress(
                nblocks=1,
                address=self.nodes[node_idx].getnewaddress()
            )
            sync_blocks(self.nodes)
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

        for divisor in map(lambda x: 2 ** x, range(2, 6)):
            tx_value = int(0.95 * UNIT / divisor)
            txns.extend([
                self.create_child_transaction(txns[-1], tx_value, 0),
                self.create_child_transaction(txns[-1], tx_value, 1)
            ])

        return txns

    def create_child_transaction(self, last_tx, tx_value, output_idx):
        tx = create_transaction(last_tx, output_idx, b'', tx_value)
        tx.vout.append(CTxOut(tx_value, CScript()))
        tx.rehash()
        return tx

    def get_tip_transactions(self, node_idx):
        node = self.nodes[node_idx]

        tip_hash = node.getbestblockhash()
        block_data = node.getblock(tip_hash)
        tx_hashes = block_data['tx']

        return tx_hashes

    def create_spendable_outputs(self):
        block = self.get_empty_block()
        self.spendable_outputs.append(block)
        return TestInstance([[block, True]], test_name='empty_block_synced')

    def get_empty_block(self):
        sync_blocks(self.nodes)
        node0 = self.nodes[0]

        hashprev = uint256_from_str(unhexlify(node0.getbestblockhash())[::-1])
        height = node0.getblockcount() + 1
        snapshot_hash = SnapshotMeta(node0.gettipsnapshot()).hash

        block = create_block(
            hashprev=hashprev,
            coinbase=create_coinbase(
                height=height,
                snapshot_hash=snapshot_hash
            )
        )
        block.solve()

        return block


if __name__ == '__main__':
    LTORTest().main()
