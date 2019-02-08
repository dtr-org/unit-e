#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
FeatureFinalizerTest tests the following:
1. Finalizer can vote after the restart
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    sync_blocks,
    assert_equal,
    wait_until,
    connect_nodes_bi,
)
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.admin import Admin


class FeatureFinalizerTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config, '-validating=1'],
        ]

    def test_finalizer_can_vote_after_restart(self):
        node = self.nodes[0]
        finalizer = self.nodes[1]

        node.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        finalizer.importmasterkey(regtest_mnemonics[1]['mnemonics'])

        # leave IBD
        node.generatetoaddress(1, node.getnewaddress())
        sync_blocks(self.nodes)

        Admin.authorize_and_disable(self, node)

        # ensure deposit is in a mempool of every node
        payto = finalizer.getnewaddress('', 'legacy')
        txid = finalizer.deposit(payto, 10000)
        self.wait_for_transaction(txid, timeout=150)

        self.restart_node(finalizer.index)

        # sync finalizer with the node when instant finalization is disabled
        node.generatetoaddress(22, node.getnewaddress())
        assert_equal(node.getblockcount(), 24)
        connect_nodes_bi(self.nodes, finalizer.index, node.index)
        sync_blocks([finalizer, node])
        assert_equal(node.getfinalizationstate()['currentEpoch'], 4)

        # finalizer should start voting for the previous epoch once new one starts
        node.generatetoaddress(1, node.getnewaddress())
        assert_equal(node.getfinalizationstate()['currentEpoch'], 5)
        wait_until(lambda: len(finalizer.getrawmempool()) > 0)

        self.log.info('test_finalizer_can_vote_after_restart passed')

    def run_test(self):
        self.test_finalizer_can_vote_after_restart()


if __name__ == '__main__':
    FeatureFinalizerTest().main()
