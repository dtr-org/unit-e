#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
EsperanzaVoteTest checks:
1. all finalizers are able to vote after every block
"""
from test_framework.util import (
    assert_equal,
    sync_blocks,
    connect_nodes,
    disconnect_nodes,
    assert_finalizationstate,
)
from test_framework.test_framework import UnitETestFramework


class EsperanzaVoteTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4

        esperanza_config = '-esperanzaconfig={"epochLength":5,"minDepositSize":1500}'
        finalizer_node_params = [
            '-validating=1',
            '-debug=all',
            '-whitelist=127.0.0.1',
            esperanza_config,
        ]
        proposer_node_params = ['-debug=all', '-whitelist=127.0.0.1', esperanza_config]

        self.extra_args = [proposer_node_params,
                           finalizer_node_params,
                           finalizer_node_params,
                           finalizer_node_params,
                           ]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        self.setup_stake_coins(*self.nodes)
        assert all(n.getbalance() == 10000 for n in self.nodes)

        # create topology where arrows denote non-persistent connection
        # finalizer1 → node0 ← finalizer2
        #                ↑
        #            finalizer3
        node0 = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]
        finalizer3 = self.nodes[3]

        connect_nodes(finalizer1, node0.index)
        connect_nodes(finalizer2, node0.index)
        connect_nodes(finalizer3, node0.index)

        # leave IBD
        node0.generatetoaddress(1, node0.getnewaddress('', 'bech32'))
        sync_blocks(self.nodes)

        # leave instant finalization
        address1 = self.nodes[1].getnewaddress("", "legacy")
        address2 = self.nodes[2].getnewaddress("", "legacy")
        address3 = self.nodes[3].getnewaddress("", "legacy")

        deptx1 = self.nodes[1].deposit(address1, 1500)
        deptx2 = self.nodes[2].deposit(address2, 2000)
        deptx3 = self.nodes[3].deposit(address3, 1500)

        self.wait_for_transaction(deptx1, timeout=10)
        self.wait_for_transaction(deptx2, timeout=10)
        self.wait_for_transaction(deptx3, timeout=10)

        disconnect_nodes(finalizer1, node0.index)
        disconnect_nodes(finalizer2, node0.index)
        disconnect_nodes(finalizer3, node0.index)
        assert_equal(len(node0.getpeerinfo()), 0)

        # move tip to the height when validators are activated
        # complete epoch + 3 epochs + 1 block of new epoch
        node0.generatetoaddress(4 + 5 + 5 + 5 + 5 + 1, node0.getnewaddress('', 'bech32'))
        assert_equal(node0.getblockcount(), 26)
        assert_finalizationstate(node0, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 3})

        # test that finalizers vote after processing 1st block of new epoch
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer3, node=node0)
        assert_equal(len(node0.getrawmempool()), 3)

        node0.generatetoaddress(1, node0.getnewaddress('', 'bech32'))
        assert_finalizationstate(node0, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 3})
        self.log.info('Finalizers voted a'
                      'fter first block of new epoch')

        # UNIT-E TODO: there is a know issue https://github.com/dtr-org/unit-e/issues/643
        # that finalizer doesn't vote after processing the checkpoint.
        # Once it's resolved, the bellow test must be uncommented
        #
        # # test that finalizers vote after processing checkpoint
        # node0.generatetoaddress(4, node0.getnewaddress('', 'bech32'))
        # assert_equal(node0.getblockcount(), 24)
        # assert_equal(len(node0.getrawmempool()), 0)
        # assert_equal(node0.getfinalizationstate()['currentEpoch'], 4)
        #
        # self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node0)
        # self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node0)
        # self.wait_for_vote_and_disconnect(finalizer=finalizer3, node=node0)
        # assert_equal(len(node0.getrawmempool()), 3)
        # self.log.info('Finalizers voted after checkpoint')


if __name__ == '__main__':
    EsperanzaVoteTest().main()
