#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    connect_nodes,
    wait_until,
)


class FullSyncTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node2 = self.nodes[2]
        node3 = self.nodes[3]

        self.setup_stake_coins(node0)

        # topology:
        # node0 -> node1 -> node2
        #      \-> node3
        connect_nodes(node0, node1.index)
        connect_nodes(node1, node2.index)
        connect_nodes(node0, node3.index)

        node0.generatetoaddress(1, node0.getnewaddress('bech32'))
        wait_until(lambda: node3.getblockcount() == 1, timeout=5)
        self.log.info('node without outbound peer received the block')

        wait_until(lambda: node1.getblockcount() == 1, timeout=5)
        self.log.info('node with outbound peer received the block')


if __name__ == '__main__':
    FullSyncTest().main()
