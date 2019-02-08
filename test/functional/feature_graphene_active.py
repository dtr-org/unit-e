#!/usr/bin/env python3
# Copyright(c) 2018 The Unit - e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http: // www.opensource.org/licenses/mit-license.php.

"""
This tests node reactions on different hand-made graphene requests/responses:

1) test_reply_block_on_getgraphene
Although node asks for graphene block - it should nevertheless accept legacy block

2) test_non_decodable_iblt
Send graphene block that can not be decoded. Node should request fallback

3) test_request_deep_blocks
It is very unlikely to reconstruct too deep graphene/compact blocks, node should
respond with legacy blocks for blocks deeper than 5 and normally for shallow
blocks
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.blocktools import (
    get_tip_snapshot_meta,
    create_block,
    create_coinbase
)
from test_framework.mininode import P2PInterface, network_thread_start
from test_framework.messages import (
    msg_headers,
    msg_block,
    GrapheneBlock,
    CBlockHeader,
    GrapheneIbltEntryDummy,
    msg_graphenblock,
    GrapheneBlockRequest,
    msg_getgraphene
)
from test_framework.util import wait_until, assert_equal
from decimal import Decimal


class GrapheneActive(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-debug=all',
                            '-whitelist=127.0.0.1',
                            '-stakesplitthreshold=1000000000'
                            ]] * self.num_nodes
        self.setup_clean_chain = True

    def run_test(self):
        self.setup_stake_coins(self.nodes[0])

        self.generate_sync(self.nodes[0])  # Exit IBD

        # The following tests will generate lots of transactions =>
        # we need lots of available UTXOs
        self.generate_sync(self.nodes[0], 10)

        self.nodes[0].add_p2p_connection(Node())
        network_thread_start()

        self.nodes[0].p2p.wait_for_verack()

        self.test_reply_block_on_getgraphene()
        self.test_non_decodable_iblt()
        self.test_request_deep_blocks()

    def test_non_decodable_iblt(self):
        self.log.info("Testing non-decodable iblt")
        count_before = self.nodes[0].getblockcount()

        p2p = self.nodes[0].p2p

        block = self.create_block()
        p2p.send_message(msg_headers([block]))
        p2p.wait_for_getgraphene(block.hash)

        graphene = GrapheneBlock()
        graphene.header = CBlockHeader(block)
        iblt_entry = GrapheneIbltEntryDummy()
        iblt_entry.count = 20000  # This ensures iblt can not be decoded
        graphene.iblt.hash_table = [iblt_entry]

        p2p.send_message(msg_graphenblock(graphene))

        # Can't decode iblt => should request fallback, which we will send
        p2p.wait_for_getdata(block.hash)
        p2p.send_message(msg_block(block))

        self.nodes[0].waitforblockheight(count_before + 1)

    def test_reply_block_on_getgraphene(self):
        self.log.info("Testing reply block on getgraphene")
        node = self.nodes[0]

        count_before = node.getblockcount()
        block = self.create_block()
        node.p2p.send_message(msg_headers([block]))
        node.p2p.wait_for_getgraphene(block.hash)
        node.p2p.send_message(msg_block(block))
        node.waitforblockheight(count_before + 1)

    def generate_with_txs(self):
        address = self.nodes[0].getnewaddress()
        for _ in range(60):
            self.nodes[0].sendtoaddress(address, Decimal("0.001"))
        return int(self.nodes[0].generate(1)[0], 16)

    def generate_empty(self):
        assert_equal(0, len(self.nodes[0].getrawmempool()))
        return int(self.nodes[0].generate(1)[0], 16)

    def test_request_deep_blocks(self):
        self.log.info("Testing deep blocks requests")

        p2p = self.nodes[0].p2p

        def ask_for(block_hash):
            request = GrapheneBlockRequest(block_hash, 100)
            p2p.send_message(msg_getgraphene(request))

        def responds_with_graphene(block_hash):
            ask_for(block_hash)
            p2p.wait_for_graphene_block(block_hash)

        def responds_with_legacy(block_hash):
            ask_for(block_hash)
            p2p.wait_for_block(block_hash)

        def responds_with_compact(block_hash):
            ask_for(block_hash)
            p2p.wait_for_compact_block(block_hash)

        blocks = [
            self.generate_with_txs(),
            self.generate_with_txs(),
            self.generate_empty(),
            self.generate_empty(),
            self.generate_with_txs(),
            self.generate_empty(),
            self.generate_with_txs()]

        self.log.info("Asking blocks")

        responds_with_legacy(blocks[-7])  # Too deep for graphene and cmpct
        responds_with_graphene(blocks[-6])
        responds_with_compact(blocks[-5])  # Empty => cmpct
        responds_with_compact(blocks[-4])  # Empty => cmpct
        responds_with_graphene(blocks[-3])
        responds_with_compact(blocks[-2])  # Empty => cmpct
        responds_with_graphene(blocks[-1])

        responds_with_compact(blocks[1])  # Already requested => not graphene
        responds_with_compact(blocks[4])  # Already requested => not graphene
        responds_with_compact(blocks[6])  # Already requested => not graphene

    def create_block(self):
        node = self.nodes[0]
        stake = node.listunspent()[0]
        tip_hash = node.getblockchaininfo()['bestblockhash']
        tip_header = node.getblockheader(tip_hash)
        snapshot_hash = get_tip_snapshot_meta(node).hash
        block = create_block(int(tip_hash, 16),
                             create_coinbase(tip_header["height"] + 1, stake,
                                             snapshot_hash),
                             tip_header["mediantime"] + 1)
        block.solve()
        block.rehash()
        return block


class Node(P2PInterface):
    def on_inv(self, _):
        pass

    def wait_for_command(self, command, msg_predicate):
        def wait_predicate():
            msg = self.last_message.get(command, None)
            if msg is None:
                return False

            return msg_predicate(msg)

        wait_until(wait_predicate, timeout=10)

    def wait_for_getgraphene(self, block_hash):
        if isinstance(block_hash, str):
            block_hash = int(block_hash, 16)

        return self.wait_for_command('getgraphene', lambda
            msg: msg.request.requested_block_hash == block_hash)

    def wait_for_getdata(self, block_hash):
        if isinstance(block_hash, str):
            block_hash = int(block_hash, 16)

        def predicate(msg):
            for inv in msg.inv:
                if inv.hash == block_hash:
                    return True

            return False

        return self.wait_for_command('getdata', predicate)

    def wait_for_block(self, block_hash):
        if isinstance(block_hash, str):
            block_hash = int(block_hash, 16)

        super().wait_for_block(block_hash)

    def wait_for_graphene_block(self, block_hash):
        if isinstance(block_hash, str):
            block_hash = int(block_hash, 16)

        def predicate(msg):
            msg.block.header.rehash()
            hash = msg.block.header.sha256
            return hash == block_hash

        return self.wait_for_command('graphenblock', predicate)

    def wait_for_compact_block(self, block_hash):
        if isinstance(block_hash, str):
            block_hash = int(block_hash, 16)

        def predicate(msg):
            msg.header_and_shortids.header.rehash()
            hash = msg.header_and_shortids.header.sha256
            return hash == block_hash

        return self.wait_for_command('cmpctblock', predicate)


if __name__ == '__main__':
    GrapheneActive().main()
