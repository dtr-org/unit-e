#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Functional tests for fork choice rule (longest justified chain).
# It also indirectly checks initial full sync implementation (commits).
# * Test that fresh chain chooses the longest justified instead, but shortest in total, chain.
# * Test that chain with more work switches to longest justified.
# * Test nodes continue to serve blocks after switch.
# * Test nodes reconnects and chose longest justified chain right after global disconnection.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
)


def setup_deposit(self, proposer, validators):
    for i, n in enumerate(validators):
        n.new_address = n.getnewaddress("", "legacy")

        assert_equal(n.getbalance(), 10000)

    for n in validators:
        deptx = n.deposit(n.new_address, 1500)
        self.wait_for_transaction(deptx)

    # the validator will be ready to operate in epoch 4
    # TODO: UNIT - E: it can be 2 epochs as soon as #572 is fixed
    generate_block(proposer, count=30)

    assert_equal(proposer.getblockcount(), 31)


class FinalizationForkChoice(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [
            ['-esperanzaconfig={"epochLength": 10, "minDepositSize": 1500}'],
            ['-esperanzaconfig={"epochLength": 10, "minDepositSize": 1500}'],
            ['-esperanzaconfig={"epochLength": 10, "minDepositSize": 1500}'],
            ['-esperanzaconfig={"epochLength": 10, "minDepositSize": 1500}', '-validating=1'],
        ]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()
        p0, p1, p2, v0 = self.nodes
        # create a connection v - p0 - p1 - v - p2
        # v0: p0, p1, p2
        # p0: v0, p1
        # p1: v0, p0
        # p2: v0
        connect_nodes(p0, p1.index)
        connect_nodes(p0, v0.index)
        connect_nodes(p1, v0.index)
        connect_nodes(p2, v0.index)

    def run_test(self):
        p0, p1, p2, v0 = self.nodes

        self.setup_stake_coins(p0, p1, p2, v0)

        # Leave IBD
        self.generate_sync(p0)

        self.log.info("Setup deposit")
        setup_deposit(self, p0, [v0])
        sync_blocks([p0, p1, p2, v0])

        self.log.info("Setup test prerequisites")
        # get to up to block 49, just one before the new checkpoint
        generate_block(p0, count=18)

        assert_equal(p0.getblockcount(), 49)
        sync_blocks([p0, p1, p2, v0])

        assert_finalizationstate(p0, {'currentEpoch': 5,
                                      'lastJustifiedEpoch': 4,
                                      'lastFinalizedEpoch': 4})

        # disconnect p0
        # v0: p1, p2
        # p0:
        # p1: v0
        # p2: v0
        disconnect_nodes(p0, v0.index)
        disconnect_nodes(p0, p1.index)

        # disconnect p2
        # v0: p1
        # p0:
        # p1: v0
        # p2:
        disconnect_nodes(p2, v0.index)

        # disconnect p1
        # v0:
        # p0:
        # p1:
        # p2:
        disconnect_nodes(p1, v0.index)

        # generate long chain in p0 but don't justify it
        #  F     F
        # 30 .. 40 .. 89    -- p0
        generate_block(p0, count=40)

        assert_equal(p0.getblockcount(), 89)
        assert_finalizationstate(p0, {'currentEpoch': 9,
                                      'lastJustifiedEpoch': 4,
                                      'lastFinalizedEpoch': 4})

        # generate short chain in p1 and justify it
        # on the 6th and 7th epochs sync with validator
        #  F     F
        # 30 .. 40 .. 49 .. .. .. .. .. .. 89    -- p0
        #               \
        #                50 .. 60 .. 69          -- p1
        #                 F
        # get to the 6th epoch
        generate_block(p1, count=2)
        self.wait_for_vote_and_disconnect(finalizer=v0, node=p1)
        # get to the 7th epoch
        generate_block(p1, count=10)
        # generate the rest of the blocks
        generate_block(p1, count=8)
        connect_nodes(p1, v0.index)
        sync_blocks([p1, v0])

        assert_equal(p1.getblockcount(), 69)
        assert_finalizationstate(p1, {'currentEpoch': 7,
                                      'lastJustifiedEpoch': 5,
                                      'lastFinalizedEpoch': 5})

        # connect p2 with p0 and p1; p2 must switch to the longest justified p1
        # v0: p1
        # p0: p2
        # p1: v0, p2
        # p2: p0, p1
        self.log.info("Test fresh node sync")
        connect_nodes(p2, p0.index)
        connect_nodes(p2, p1.index)

        sync_blocks([p1, p2])
        assert_equal(p1.getblockcount(), 69)
        assert_equal(p2.getblockcount(), 69)

        assert_finalizationstate(p1, {'currentEpoch': 7,
                                      'lastJustifiedEpoch': 5,
                                      'lastFinalizedEpoch': 5})
        assert_finalizationstate(p2, {'currentEpoch': 7,
                                      'lastJustifiedEpoch': 5,
                                      'lastFinalizedEpoch': 5})

        # connect p0 with p1, p0 must disconnect its longest but not justified fork and choose p1
        # v0: p1
        # p0: p1, p2
        # p1: v0, p0, p2
        # p2: p0, p1
        self.log.info("Test longest node reverts to justified")
        connect_nodes(p0, p1.index)
        sync_blocks([p0, p1])

        # check if p0 accepted shortest in terms of blocks but longest justified chain
        assert_equal(p0.getblockcount(), 69)
        assert_equal(p1.getblockcount(), 69)
        assert_equal(v0.getblockcount(), 69)

        # generate more blocks to make sure they're processed
        self.log.info("Test all nodes continue to work as usual")
        generate_block(p0, count=30)
        sync_blocks([p0, p1, p2, v0])
        assert_equal(p0.getblockcount(), 99)

        generate_block(p1, count=30)
        sync_blocks([p0, p1, p2, v0])
        assert_equal(p1.getblockcount(), 129)

        generate_block(p2, count=30)
        sync_blocks([p0, p1, p2, v0])
        assert_equal(p2.getblockcount(), 159)

        # disconnect all nodes
        # v0:
        # p0:
        # p1:
        # p2:
        self.log.info("Test nodes sync after reconnection")
        disconnect_nodes(v0, p1.index)
        disconnect_nodes(p0, p1.index)
        disconnect_nodes(p0, p2.index)
        disconnect_nodes(p1, p2.index)

        generate_block(p0, count=10)
        generate_block(p1, count=20)
        generate_block(p2, count=30)

        assert_equal(p0.getblockcount(), 169)
        assert_equal(p1.getblockcount(), 179)
        assert_equal(p2.getblockcount(), 189)

        # connect validator back to p1
        # v0: p1
        # p0: p1
        # p1: v0, p0, p2
        # p2: p1
        connect_nodes(p1, v0.index)
        sync_blocks([p1, v0])
        connect_nodes(p1, p0.index)
        connect_nodes(p1, p2.index)
        sync_blocks([p0, p1, p2, v0])


if __name__ == '__main__':
    FinalizationForkChoice().main()
