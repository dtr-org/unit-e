#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test p2p commits messaging.
    1. CommitsTest.getcommits_test: generate blocks on the node and test getcommits behavior
    2. CommitsTest.commits_test: send commits to the node and check its state
"""
from test_framework.blocktools import (
    create_coinbase,
    create_block,
)
from test_framework.test_framework import UnitETestFramework
from test_framework.key import CECKey
from test_framework.messages import (
    msg_getcommits,
    msg_commits,
    CBlockHeader,
    CommitsLocator,
    HeaderAndCommits,
)
from test_framework.util import (
    assert_equal,
    wait_until,
)
import time
from test_framework.mininode import (
    P2PInterface,
)

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


class CommitsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [[ '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig={"epochLength": 5}' ]] * 2
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):

        self.setup_stake_coins(self.nodes[0])

        for n in self.nodes:
            n.add_p2p_connection(P2P())
        for n in self.nodes:
            n.p2p.wait_for_verack()
        self.getcommits_test(self.nodes[0])
        self.commits_test(self.nodes[1])

    def getcommits_test(self, node):
        p2p = node.p2p
        blocks = [0]

        def generate(n):
            for hash in node.generate(n):
                blocks.append(int(hash, 16))

        def getcommits(start, stop=0):
            node.p2p.reset_messages()
            node.p2p.send_message(msg_getcommits(CommitsLocator(start, stop)))

        def check_commits(status, hashes):
            wait_until(lambda: len(p2p.messages) > 0, timeout=5)
            m = p2p.messages[0]
            assert_equal(m.status, status)
            assert_equal(len(m.data), len(hashes))
            for i in range(0, len(hashes)):
                assert_equal(m.data[i].header.sha256, hashes[i])

        generate(18)
        # When no validators present, node automatically justifies previous epoch.
        # So that:
        # F      F      J
        # 0..4 - 5..9 - 10..14 - 15..18
        assert_equal(node.getblockcount(), 18)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 1)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 2)

        getcommits([blocks[5]])  # expect error: not a checkpoint
        time.sleep(2)
        assert_equal(len(p2p.messages), 0)

        getcommits([blocks[4]])
        check_commits(0, blocks[5:10])

        getcommits([blocks[4], blocks[9]])
        check_commits(1, blocks[10:19])

        getcommits([blocks[4], blocks[12]])
        check_commits(1, blocks[13:19])

        getcommits([blocks[4], blocks[9], blocks[11]])
        check_commits(1, blocks[12:19])

        # ascend ordering is broken, 11 is considered biggest
        getcommits([blocks[4], blocks[11], blocks[9]])
        check_commits(1, blocks[12:19])

        # ascend ordering is broken, 11 is considered biggest, 12 is shadowed
        getcommits([blocks[4], blocks[11], blocks[9], blocks[12]])
        check_commits(1, blocks[12:19])

        generate(1)  # 19th block, non-finalized checkpoint
        assert_equal(node.getblockcount(), 19)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 1)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 2)

        getcommits([blocks[14]])  # expect error
        time.sleep(2)
        assert_equal(len(p2p.messages), 0)

        # last epoch is full but still not finalized, expect status=1
        getcommits([blocks[9]])
        check_commits(1, blocks[10:20])

        getcommits([blocks[14]])  # expect error: not finalized checkpoint
        time.sleep(2)
        assert_equal(len(p2p.messages), 0)

        generate(1)
        assert_equal(node.getblockcount(), 20)
        assert_equal(node.getfinalizationstate()['lastFinalizedEpoch'], 2)
        assert_equal(node.getfinalizationstate()['lastJustifiedEpoch'], 3)

        # Epoch 15..19 is now finalized, expect status=0
        getcommits([blocks[9]])
        check_commits(0, blocks[10:15])

        getcommits([blocks[14]])
        check_commits(1, blocks[15:21])

        # Ask for unknown block hash, check most recent block is 14.
        getcommits([blocks[9], blocks[14], 0x4242424242])
        check_commits(1, blocks[15:21])

    def commits_test(self, node):
        def check_headers(number):
            wait_until(lambda: node.getblockchaininfo()['headers'] == number, timeout=5)

        def check_reject(err, block):
            wait_until(lambda: node.p2p.has_reject(err, block), timeout=5)

        def getbestblockhash():
            return int(node.getbestblockhash(), 16)

        def make_block(prev=None, secret=None):
            if secret is None:
                secret = "default"
            coinbase_key = CECKey()
            coinbase_key.set_secretbytes(bytes(secret, "utf-8"))
            coinbase_pubkey = coinbase_key.get_pubkey()
            if prev is None:
                block_base_hash = getbestblockhash()
                block_time = int(time.time()) + 1
            else:
                block_base_hash = prev.sha256
                block_time = prev.nTime + 1
            height = prev.height + 1 if prev else 1
            snapshot_hash = 0
            stake = self.nodes[0].listunspent()[0]
            coinbase = create_coinbase(height, stake, snapshot_hash, coinbase_pubkey)
            coinbase.rehash()
            b = create_block(block_base_hash, coinbase, block_time)
            b.solve()
            b.height = height
            return b

        def make_commits_msg(blocks):
            msg = msg_commits(0)
            for b in blocks:
                hc = HeaderAndCommits()
                hc.header = CBlockHeader(b)
                msg.data += [hc]
            return msg

        def send_commits(blocks):
            node.p2p.reset_messages()
            node.p2p.send_message(make_commits_msg(blocks))

        chain = []
        def generate(n):
            tip = chain[-1] if len(chain) > 0 else None
            for i in range(0, n):
                tip = make_block(tip)
                chain.append(tip)

        check_headers(0) # initial state of the node

        # generate 10 blocks and send commits
        generate(10)
        send_commits(chain)
        check_headers(10) # node accepted 10 headers

        # send same commits again
        send_commits(chain)
        check_headers(10)

        # send last 5 commits
        send_commits(chain[-5:])
        check_headers(10)

        # generate next 10 blocks, try to send commits starting from 2nd block
        generate(10)
        send_commits(chain[11:])
        check_reject(b'prev-blk-not-found', 0)  # node rejected orphan headers
        check_headers(10) # must keep old amount of headers

        # send correct commits
        send_commits(chain[10:])
        check_headers(20) # node accepted headers

        # generate next 10 blocks, send whole chain
        generate(10)
        send_commits(chain)
        check_headers(30) # node accepted headers

        # generate next 10 blocks, fool commit in one of them, send them
        generate(10)
        msg = make_commits_msg(chain[-10:])
        msg.data[-1].commits = chain[-1].vtx # fool commits with coinbase tx
        node.p2p.send_message(msg)
        check_reject(b'bad-non-commit', chain[-1].sha256) # node rejected commits because of non-commit transaction
        check_headers(30) # must keep old amount of headers


if __name__ == '__main__':
    CommitsTest().main()
