#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
VoteTest checks:
1. all finalizers are able to vote after every block
2. finalizers delay voting according to -finalizervotefromepochblocknumber
"""
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    make_vote_tx,
    sync_blocks,
)
from test_framework.test_framework import UnitETestFramework


class VoteTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4

        self.extra_args = [
            [],
            ['-validating=1'],
            ['-validating=1'],
            ['-validating=1'],
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
        generate_block(node0)
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

        # move tip to the height when finalizers are activated
        # complete epoch + 2 epochs + 1 block of new epoch
        generate_block(node0, count=4 + 5 + 5 + 1)
        assert_equal(node0.getblockcount(), 16)
        assert_finalizationstate(node0, {'currentDynasty': 2,
                                         'currentEpoch': 4,
                                         'lastJustifiedEpoch': 2,
                                         'lastFinalizedEpoch': 2,
                                         'validators': 3})

        # test that finalizers vote after processing 1st block of new epoch
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer3, node=node0)
        assert_equal(len(node0.getrawmempool()), 3)

        generate_block(node0, count=4)
        assert_equal(node0.getblockcount(), 20)
        assert_finalizationstate(node0, {'currentDynasty': 2,
                                         'currentEpoch': 4,
                                         'lastJustifiedEpoch': 3,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 3})
        self.log.info('Finalizers voted after first block of new epoch')

        # test that finalizers can vote on a configured epoch block number
        self.restart_node(finalizer1.index, ['-validating=1', '-finalizervotefromepochblocknumber=1'])
        self.restart_node(finalizer2.index, ['-validating=1', '-finalizervotefromepochblocknumber=2'])
        self.restart_node(finalizer3.index, ['-validating=1', '-finalizervotefromepochblocknumber=3'])

        generate_block(node0)
        assert_equal(node0.getblockcount(), 21)
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node0)
        connect_nodes(finalizer2, node0.index)
        connect_nodes(finalizer3, node0.index)
        sync_blocks([finalizer2, finalizer3, node0], timeout=10)
        assert_equal(len(node0.getrawmempool()), 1)  # no votes from finalizer2 and finalizer3
        disconnect_nodes(finalizer2, node0.index)
        disconnect_nodes(finalizer3, node0.index)

        generate_block(node0)
        assert_equal(node0.getblockcount(), 22)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node0)
        connect_nodes(finalizer3, node0.index)
        sync_blocks([finalizer3, node0], timeout=10)
        assert_equal(len(node0.getrawmempool()), 1)  # no votes from finalizer3
        disconnect_nodes(finalizer3, node0.index)

        generate_block(node0)
        assert_equal(node0.getblockcount(), 23)
        self.wait_for_vote_and_disconnect(finalizer=finalizer3, node=node0)
        generate_block(node0, count=2)
        assert_equal(node0.getblockcount(), 25)
        assert_finalizationstate(node0, {'currentDynasty': 3,
                                         'currentEpoch': 5,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 3})
        self.log.info('Finalizers voted on a configured block number')

        # test that finalizers can vote after configured epoch block number
        generate_block(node0, count=4)
        assert_equal(node0.getblockcount(), 29)
        prev_tx = self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer3, node=node0)
        generate_block(node0)
        assert_equal(node0.getblockcount(), 30)
        assert_finalizationstate(node0, {'currentDynasty': 4,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 5,
                                         'validators': 3})
        self.log.info('Finalizers voted after configured block number')

        generate_block(node0, count=4)
        prev_tx = finalizer1.decoderawtransaction(prev_tx)['txid']

        # check that make_vote_tx works as expected
        tx = make_vote_tx(finalizer1, address1, node0.getblockhash(30), 5, 6, prev_tx)
        node0.sendrawtransaction(tx)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=node0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer3, node=node0)
        generate_block(node0)
        assert_equal(node0.getblockcount(), 35)
        assert_finalizationstate(node0, {'currentDynasty': 5,
                                         'currentEpoch': 7,
                                         'lastJustifiedEpoch': 6,
                                         'lastFinalizedEpoch': 6,
                                         'validators': 3})
        self.log.info('make_vote_tx works together with real finalizers')

        # test that node recognizes old and invalid votes.
        tx = make_vote_tx(finalizer1, address1, node0.getblockhash(30), 1, 2, prev_tx)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', node0.sendrawtransaction, tx)
        tx = make_vote_tx(finalizer1, address1, node0.getblockhash(30), 2, 3, prev_tx)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', node0.sendrawtransaction, tx)
        tx = make_vote_tx(finalizer1, address1, node0.getblockhash(30), 7, 9, prev_tx)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', node0.sendrawtransaction, tx)
        tx = make_vote_tx(finalizer1, address1, node0.getblockhash(30), 7, 6, prev_tx)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', node0.sendrawtransaction, tx)
        self.log.info('Tested outdated and invalid vote votes')

        # UNIT-E TODO: there is a know issue https://github.com/dtr-org/unit-e/issues/643
        # that finalizer doesn't vote after processing the checkpoint.
        # Once it's resolved, the bellow test must be uncommented
        #
        # # test that finalizers vote after processing checkpoint
        # generate_block(node0, count=4)
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
    VoteTest().main()
