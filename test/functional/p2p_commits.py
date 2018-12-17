#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.messages import (
    msg_getcommits,
    msg_commits,
    CommitsLocator,
    HeaderAndCommits,
)
from test_framework.mininode import network_thread_start, P2PInterface
from test_framework.test_framework import UnitETestFramework
from test_framework.key import CECKey
from test_framework.util import (
    assert_equal,
    wait_until,
)
import time
from test_framework.blocktools import *

class P2P(P2PInterface):
    def __init__(self):
        super().__init__()
        self.messages = []

    def reset_messages(self):
        self.messages = []

    def on_commits(self, msg):
        self.messages.append(msg)


class GetCommitsTest(UnitETestFramework):
    def __init__(self):
        super().__init__()
        self.blocks = [0]

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[ '-printtoconsole', '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig={"epochLength": 5}' ]]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def generate(self, n):
        for i in range(len(self.blocks), len(self.blocks) + n):
            self.blocks += [int(self.nodes[0].generate(1)[0], 16)]

    def getcommits(self, start, stop=0):
        self.nodes[0].p2p.reset_messages()
        self.nodes[0].p2p.send_message(msg_getcommits(CommitsLocator(start, stop)))

    def check_commits(self, status, hashes):
        p2p = self.nodes[0].p2p
        wait_until(lambda: len(p2p.messages) > 0, timeout=5)
        m = p2p.messages[0]
        assert_equal(m.status, status)
        assert_equal(len(m.data), len(hashes))
        for i in range(0, len(hashes)):
            assert_equal(m.data[i].header.sha256, hashes[i])

    def run_test(self):
        p = self.nodes[0]
        p2p = P2P()
        p.add_p2p_connection(p2p)
        network_thread_start()

        self.generate(13);
        # When no validators present, node automatically justifies and finalize every
        # previous epoch. So that:
        # 4 is justified and finalized
        # 9 is justified and finalized
        # Last epoch is 10..13, not finalized

        self.getcommits([self.blocks[5]]) # expect error: not a checkpoint
        time.sleep(2)
        assert_equal(len(p2p.messages), 0)

        self.getcommits([self.blocks[4]])
        self.check_commits(0, self.blocks[5:10])

        self.getcommits([self.blocks[4], self.blocks[9]])
        self.check_commits(1, self.blocks[10:14])

        self.getcommits([self.blocks[4], self.blocks[12]])
        self.check_commits(1, self.blocks[13:14])

        self.getcommits([self.blocks[4], self.blocks[9], self.blocks[11]])
        self.check_commits(1, self.blocks[12:14])

        # ascend ordering is broken, 11 is considered biggest
        self.getcommits([self.blocks[4], self.blocks[11], self.blocks[9]])
        self.check_commits(1, self.blocks[12:14])

        # ascend ordering is broken, 11 is considered biggest, 12 is shadowed
        self.getcommits([self.blocks[4], self.blocks[11], self.blocks[9], self.blocks[12]]) #expect [12..13]
        self.check_commits(1, self.blocks[12:14])

        self.generate(1); # 14th block, unfinalized checkpoint
        self.getcommits([self.blocks[14]])  # expect error
        time.sleep(2)
        assert_equal(len(p2p.messages), 0)

        # last epoch is full but still not finalized, expect status=1
        self.getcommits([self.blocks[9]])
        self.check_commits(1, self.blocks[10:15])

        self.getcommits([self.blocks[14]]) # expect error: not finalized checkpoint
        time.sleep(2)
        assert_equal(len(p2p.messages), 0)

        self.generate(1); # 15th block
        # Epoch 10..14 is now finalized, expect status=0
        self.getcommits([self.blocks[9]])
        self.check_commits(0, self.blocks[10:15])

        self.getcommits([self.blocks[14]])
        self.check_commits(1, [self.blocks[15]])

        # Ask for unknown block hash, check most recent block is 14.
        self.getcommits([self.blocks[9], self.blocks[14], 0x4242424242])
        self.check_commits(1, [self.blocks[15]])

class CommitsTest(UnitETestFramework):
    def __init__(self):
        super().__init__()
        self.blocks = {}

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[ '-printtoconsole', '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig={"epochLength": 5}' ]];
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def getbestblockhash(self):
        return int(self.nodes[0].getbestblockhash(), 16)

    def create_block(self, prev=None, secret=None):
        if secret is None:
            secret = "default"
        coinbase_key = CECKey()
        coinbase_key.set_secretbytes(bytes(secret, "utf-8"))
        coinbase_pubkey = coinbase_key.get_pubkey()
        if prev is None:
            block_base_hash = self.getbestblockhash()
            block_time = int(time.time()) + 1
        else:
            block_base_hash = prev.sha256
            block_time = prev.nTime + 1
        height = prev.height + 1 if prev else 1
        snapshot_hash = 0
        coinbase = create_coinbase(height, snapshot_hash, coinbase_pubkey)
        coinbase.rehash()
        b = create_block(block_base_hash, coinbase, block_time)
        b.solve()
        b.height = height
        return b

    def make_commits_msg(self, blocks):
        msg = msg_commits(0);
        for b in blocks:
            hc = HeaderAndCommits()
            hc.header = CBlockHeader(b)
            msg.data += [hc]
        return msg

    def send_commits(self, blocks):
        self.nodes[0].p2p.send_message(self.make_commits_msg(blocks))

    def check_headers(self, number):
        info = self.nodes[0].getblockchaininfo()
        assert_equal(info['headers'], number)

    def run_test(self):
        p = self.nodes[0]
        p2p = P2P()
        p.add_p2p_connection(p2p)
        network_thread_start()

        chain = []
        tip = lambda c: c[-1] if len(c) > 0 else None

        self.check_headers(0) # initial state of the node

        # generate 10 blocks and send commits
        for i in range(0, 10):
            chain.append(self.create_block(tip(chain)))

        self.send_commits(chain)
        self.check_headers(10) # node accepted 10 headers

        # send same commits again
        self.send_commits(chain)
        self.check_headers(10)

        # send last 5 commits
        self.send_commits(chain[-5:])
        self.check_headers(10)

        # generate next 10 blocks, try to send commits starting from 2nd block
        for i in range(0, 10):
            chain.append(self.create_block(tip(chain)))

        self.send_commits(chain[11:])
        self.check_headers(10) # node rejected orphan headers

        # send correct commits
        self.send_commits(chain[10:])
        self.check_headers(20) # node accepted headers

        # generate next 10 blocks, send whole chain
        for i in range(0, 10):
            chain.append(self.create_block(tip(chain)))

        self.send_commits(chain)
        self.check_headers(30) # node accepted headers

        # generate next 10 blocks, fool commit in one of them, send them
        for i in range(0, 10):
            chain.append(self.create_block(tip(chain)))
        msg = self.make_commits_msg(chain[-10:])
        msg.data[-1].commits = chain[-1].vtx # fool commits with coinbase tx
        self.nodes[0].p2p.send_message(msg)
        self.check_headers(30) # node rejected commits because of non-commit transaction


if __name__ == '__main__':
    GetCommitsTest().main()
    CommitsTest().main()
