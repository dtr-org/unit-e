#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    connect_nodes_bi,
    connect_nodes,
    disconnect_nodes,
    assert_equal,
    sync_blocks,
    wait_until,
)
from test_framework.admin import Admin
from test_framework.regtest_mnemonics import regtest_mnemonics


class ForkChoiceFinalizationTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def test_fork_inside_dynasty(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node2 = self.nodes[2]
        validator = self.nodes[3]

        node0.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        validator.importmasterkey(regtest_mnemonics[1]['mnemonics'])

        # connect node0 to every peer for fast propagation of admin transactions
        connect_nodes_bi(self.nodes, node0.index, node1.index)
        connect_nodes_bi(self.nodes, node0.index, node2.index)
        connect_nodes_bi(self.nodes, node0.index, validator.index)

        # leave IBD
        node0.generatetoaddress(1, node0.getnewaddress())
        self.sync_all()

        Admin.authorize_and_disable(self, node0)

        payto = validator.getnewaddress('', 'legacy')
        txid = validator.deposit(payto, 10000)
        self.wait_for_transaction(txid, timeout=150)

        self.restart_node(node0.index)
        self.restart_node(node1.index)
        self.restart_node(node2.index)

        # 0 ... 4 ... 9 ... 14 ... 19 ... 24 - 25
        #       F     F     F                  tip
        node0.generatetoaddress(23, node0.getnewaddress())
        assert_equal(node0.getblockcount(), 25)
        assert_equal(node0.getfinalizationstate()['currentEpoch'], 5)
        assert_equal(node0.getfinalizationstate()['currentDynasty'], 4)
        assert_equal(node0.getfinalizationstate()['lastFinalizedEpoch'], 3)
        assert_equal(node0.getfinalizationstate()['lastJustifiedEpoch'], 3)
        assert_equal(node0.getfinalizationstate()['validators'], 1)

        connect_nodes(node0, node1.index)
        connect_nodes(node0, node2.index)
        sync_blocks([node0, node1, node2])
        disconnect_nodes(node0, node1.index)
        disconnect_nodes(node0, node2.index)

        # generate fork with no commits. node0 must switch to it
        # node1 25
        #        \
        #         - b26_0 node0, node2
        b26_0 = node2.generatetoaddress(1, node2.getnewaddress())[0]
        connect_nodes(node0, node2.index)
        sync_blocks([node0, node2])
        disconnect_nodes(node0, node2.index)

        # generate fork with justified commits. node0 must switch to it
        #    - b26_1 node0, node1
        #   /
        # 25
        #   \
        #    - b26_0 node2
        connect_nodes(node1, validator.index)
        sync_blocks([node1, validator])
        wait_until(lambda: len(node1.getrawmempool()) > 0, timeout=150)
        b26_1 = node1.generatetoaddress(1, node1.getnewaddress())[0]
        sync_blocks([node1, validator])
        disconnect_nodes(node1, validator.index)
        self.log.info('node successfully switched to longest justified fork')

        assert_equal(node0.getblockhash(26), b26_0)
        connect_nodes(node1, node0.index)
        sync_blocks([node1, node0])
        disconnect_nodes(node1, node0.index)
        assert_equal(node0.getblockhash(26), b26_1)
        assert_equal(node0.getfinalizationstate()['lastJustifiedEpoch'], 5)

        # generate longer but not justified fork. node0 shouldn't switch
        #    - b26_1 node0, node1, node2
        #   /
        # 25
        #   \
        #    - b26_0 - 27
        node2.generatetoaddress(1, node2.getnewaddress())
        connect_nodes(node2, node0.index)
        sync_blocks([node2, node0])
        disconnect_nodes(node2, node0.index)
        assert_equal(node0.getfinalizationstate()['lastJustifiedEpoch'], 5)
        assert_equal(node2.getfinalizationstate()['lastJustifiedEpoch'], 5)
        self.log.info('node did not switch to heaviest but less justified fork')

        self.log.info('test_fork_inside_dynasty passed')

    def run_test(self):
        self.test_fork_inside_dynasty()


if __name__ == '__main__':
    ForkChoiceFinalizationTest().main()
