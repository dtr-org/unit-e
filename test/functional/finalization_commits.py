#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.messages import msg_getcommits, CCommitsLocator
from test_framework.mininode import network_thread_start, P2PInterface
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal

class P2P(P2PInterface):
    def __init__(self):
        super().__init__()

class CommitsTest(UnitETestFramework):
    def __init__(self):
        super().__init__()
        self.blocks = [0]

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[ '-printtoconsole', '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig={"epochLength": 5}' ]];
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def generate(self, n):
        for i in range(len(self.blocks), len(self.blocks) + n):
            self.blocks += [int(self.nodes[0].generate(1)[0], 16)]

    def getcommits(self, start, stop=0):
        self.nodes[0].p2p.send_message(msg_getcommits(CCommitsLocator(start, stop)))

    def run_test(self):
        p = self.nodes[0]
        p.add_p2p_connection(P2P())
        network_thread_start()

        self.generate(14);
        # When no validators present, node automatically justifies and finalize every
        # previous epoch. So that:
        # 5 is justified and finalized
        # 10 is not (15th block will justify and finalize it)

        self.getcommits([self.blocks[5]])  # expect [6..14]
        self.getcommits([self.blocks[10]]) # expect error
        self.getcommits([self.blocks[5], self.blocks[10]]) #expect [11..14]
        self.getcommits([self.blocks[5], self.blocks[13]]) #expect [14]
        self.getcommits([self.blocks[5], self.blocks[10], self.blocks[12]]) #expect [13..14]

        # ascend ordering is broken, 12 is considered biggest
        self.getcommits([self.blocks[5], self.blocks[12], self.blocks[10]]) #expect [13..14]

        # ascend ordering is broken, 12 is considered biggest, 13 is shadowed
        self.getcommits([self.blocks[5], self.blocks[12], self.blocks[10], self.blocks[13]]) #expect [13..14]

if __name__ == '__main__':
     CommitsTest().main()
