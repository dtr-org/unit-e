#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
FinalizatioSelfSlashTest checks:
1. The finalizer will never slash itself
"""
from test_framework.test_framework import UnitETestFramework
from test_framework.blocktools import (
    TxType,
)
from test_framework.util import (
    assert_finalizationstate,
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
    sync_mempools,
    wait_until,
)


class FinalizationSlashSelfTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [
            [],
            [],
            ['-validating=1'],
            ['-validating=1'],
        ]
        self.setup_clean_chain = True

    # initial topology where arrows denote the direction of connections
    # finalizer2 ← fork1 → finalizer1
    #                ↓  ︎
    #              fork2
    def setup_network(self):
        self.setup_nodes()

        fork1 = self.nodes[0]
        fork2 = self.nodes[1]
        finalizer1 = self.nodes[2]
        finalizer2 = self.nodes[3]

        connect_nodes(fork1, fork2.index)
        connect_nodes(fork1, finalizer1.index)
        connect_nodes(fork1, finalizer2.index)

    def test_double_votes(self):
        fork1 = self.nodes[0]
        fork2 = self.nodes[1]
        finalizer1 = self.nodes[2]
        finalizer2 = self.nodes[3]

        self.setup_stake_coins(fork1, fork2, finalizer1)

        # clone finalizer1
        finalizer2.importmasterkey(finalizer1.mnemonics)

        # leave IBD
        generate_block(fork1)
        sync_blocks([fork1, fork2, finalizer1, finalizer2])

        disconnect_nodes(fork1, finalizer2.index)
        addr = finalizer1.getnewaddress('', 'legacy')
        txid1 = finalizer1.deposit(addr, 1500)
        wait_until(lambda: txid1 in fork1.getrawmempool())

        txid2 = finalizer2.deposit(addr, 1500)
        assert_equal(txid1, txid2)
        connect_nodes(fork1, finalizer2.index)

        generate_block(fork1)
        sync_blocks([fork1, fork2, finalizer1, finalizer2])
        disconnect_nodes(fork1, finalizer1.index)
        disconnect_nodes(fork1, finalizer2.index)

        # pass instant finalization
        # F    F    F
        # e0 - e1 - e2 - e3 - e4[16] fork1, fork2
        generate_block(fork1, count=3 + 5 + 5 + 1)
        assert_equal(fork1.getblockcount(), 16)
        assert_finalizationstate(fork1, {'currentEpoch': 4,
                                         'lastJustifiedEpoch': 2,
                                         'lastFinalizedEpoch': 2,
                                         'validators': 1})

        # change topology where forks are not connected
        # finalizer1 → fork1
        #
        # finalizer2 → fork2
        sync_blocks([fork1, fork2])
        disconnect_nodes(fork1, fork2.index)

        # Create some blocks and cause finalizer to vote, then take the vote and send it to
        # finalizer2, when finalizer2 will vote it should not slash itself
        #                            v1          v2a
        #                          - e5 - e6[26, 27, 28] - e7[31] fork1
        # F    F    F    F    F   /
        # e0 - e1 - e2 - e3 - e4[16]
        #                         \  v1          v2a
        #                          - e5 - e6[26, 27] fork2
        generate_block(fork1)
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=fork1)
        generate_block(fork1, count=5)
        raw_vote_1 = self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=fork1)
        generate_block(fork1)
        assert_equal(fork1.getblockcount(), 23)
        assert_finalizationstate(fork1, {'currentEpoch': 5,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        # We'll use a second vote to check if there is slashing when a validator tries to send a double vote after it
        # voted.
        generate_block(fork1, count=3)
        raw_vote_2 = self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=fork1)
        assert_equal(fork1.getblockcount(), 26)
        assert_finalizationstate(fork1, {'currentEpoch': 6,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        # Send the conflicting vote from the other chain to finalizer2, it should record it and slash it later
        assert_raises_rpc_error(-26, "bad-vote-invalid", finalizer2.sendrawtransaction, raw_vote_1)

        generate_block(fork2)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork2)
        generate_block(fork2, count=5)
        assert_equal(fork2.getblockcount(), 22)
        assert_finalizationstate(fork2, {'currentEpoch': 5,
                                         'lastJustifiedEpoch': 3,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        connect_nodes(finalizer2, fork2.index)
        wait_until(lambda: len(finalizer2.getrawmempool()) == 1, timeout=10)
        sync_mempools([fork2, finalizer2])
        assert_equal(len(fork2.getrawmempool()), 1)

        # check that a vote, and not a slash is actually in the mempool
        vote = fork2.decoderawtransaction(fork2.getrawtransaction(fork2.getrawmempool()[0]))
        assert_equal(vote['txtype'], TxType.VOTE.value)

        fork2.generatetoaddress(1, fork1.getnewaddress('', 'bech32'))
        assert_equal(len(fork2.getrawmempool()), 0)
        disconnect_nodes(finalizer2, fork2.index)

        # check if there is slashing after voting
        fork2.generatetoaddress(3, fork1.getnewaddress('', 'bech32'))
        assert_equal(fork2.getblockcount(), 26)
        assert_finalizationstate(fork2, {'currentEpoch': 6,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork2)

        assert_raises_rpc_error(-26, "bad-vote-invalid", finalizer2.sendrawtransaction, raw_vote_2)

        # The vote hasn't been replaces by a slash
        vote = finalizer2.decoderawtransaction(finalizer2.getrawtransaction(finalizer2.getrawmempool()[0]))
        assert_equal(vote['txtype'], TxType.VOTE.value)

    def run_test(self):
        self.test_double_votes()


if __name__ == '__main__':
    FinalizationSlashSelfTest().main()
