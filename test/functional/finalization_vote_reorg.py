#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
VoteReorgTest checks that finalizer
votes correctly during re-orgs
"""
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
)
from test_framework.test_framework import UnitETestFramework
from test_framework.messages import (
    CTransaction,
    FromHex,
)


class VoteReorgTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [
            [],
            [],
            ['-validating=1'],
            ['-validating=1'],
        ]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def assert_vote(vote_raw_tx, input_raw_tx, source_epoch, target_epoch, target_hash):
            vote_tx = FromHex(CTransaction(), vote_raw_tx)
            assert vote_tx.is_finalizer_commit()

            input_tx = FromHex(CTransaction(), input_raw_tx)
            input_tx.rehash()
            prevout = "%064x" % vote_tx.vin[0].prevout.hash
            assert_equal(prevout, input_tx.hash)

            vote = self.nodes[0].extractvotefromsignature(bytes_to_hex_str(vote_tx.vin[0].scriptSig))
            assert_equal(vote['source_epoch'], source_epoch)
            assert_equal(vote['target_epoch'], target_epoch)
            assert_equal(vote['target_hash'], target_hash)

        fork0 = self.nodes[0]
        fork1 = self.nodes[1]
        finalizer = self.nodes[2]  # main finalizer that being checked
        finalizer2 = self.nodes[3]  # secondary finalizer to control finalization

        self.setup_stake_coins(fork0, fork1, finalizer, finalizer2)

        connect_nodes(fork0, fork1.index)
        connect_nodes(fork0, finalizer.index)
        connect_nodes(fork0, finalizer2.index)

        # leave IBD
        generate_block(fork0)
        sync_blocks(self.nodes)

        # deposit
        d1_hash = finalizer.deposit(finalizer.getnewaddress('', 'legacy'), 1500)
        d2_hash = finalizer2.deposit(finalizer2.getnewaddress('', 'legacy'), 4000)
        d1 = finalizer.getrawtransaction(d1_hash)
        self.wait_for_transaction(d1_hash, timeout=10)
        self.wait_for_transaction(d2_hash, timeout=10)
        generate_block(fork0)
        disconnect_nodes(fork0, finalizer.index)
        disconnect_nodes(fork0, finalizer2.index)

        # leave instant justification
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6[26]
        generate_block(fork0, count=3 + 5 + 5 + 5 + 5 + 1)
        assert_equal(fork0.getblockcount(), 26)
        assert_finalizationstate(fork0, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 2})

        # move tip to one block before checkpoint to be able to
        # revert checkpoint on the fork
        #       J           v0
        # ... - e5 - e6[26, 27, 28, 29] fork0
        #                            \
        #                             - fork1
        v0 = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork0)
        assert_vote(vote_raw_tx=v0, input_raw_tx=d1, source_epoch=4, target_epoch=5, target_hash=fork0.getblockhash(25))
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork0)
        generate_block(fork0, count=3)
        sync_blocks([fork0, fork1], timeout=10)
        disconnect_nodes(fork0, fork1.index)
        assert_equal(fork0.getblockcount(), 29)
        assert_finalizationstate(fork0, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 2})

        # vote v1 on target_epoch=6 target_hash=30
        #       J           v0                       v1
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7[31, 32] fork0
        #                            \
        #                             - fork1
        generate_block(fork0, count=2)
        assert_equal(fork0.getblockcount(), 31)
        v1 = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork0)
        assert_vote(vote_raw_tx=v1, input_raw_tx=v0, source_epoch=5, target_epoch=6, target_hash=fork0.getblockhash(30))
        generate_block(fork0)
        connect_nodes(finalizer, fork0.index)
        sync_blocks([finalizer, fork0], timeout=10)
        disconnect_nodes(finalizer, fork0.index)
        assert_equal(fork0.getblockcount(), 32)
        assert_equal(finalizer.getblockcount(), 32)
        self.log.info('finalizer successfully voted on the checkpoint')

        # re-org last checkpoint and check that finalizer doesn't vote
        #       J           v0                       v1
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7[31, 32] fork0
        #                            \
        #                             - 30] - e7[31, 32, 33] fork1
        generate_block(fork1, count=4)
        assert_equal(fork1.getblockcount(), 33)
        connect_nodes(finalizer, fork1.index)
        sync_blocks([finalizer, fork1], timeout=10)
        assert_equal(finalizer.getblockcount(), 33)
        assert_equal(len(fork1.getrawmempool()), 0)
        disconnect_nodes(finalizer, fork1.index)
        self.log.info('finalizer successfully detected potential double vote and did not vote')

        # continue to new epoch and check that finalizer votes on fork1
        #       J           v0                       v1
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7[31, 32] fork0
        #                            \                         v2
        #                             - 30] - e7[...] - e8[36, 37] fork1
        generate_block(fork1, count=3)
        assert_equal(fork1.getblockcount(), 36)
        v2 = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork1)
        assert_vote(vote_raw_tx=v2, input_raw_tx=v0, source_epoch=5, target_epoch=7, target_hash=fork1.getblockhash(35))
        generate_block(fork1)
        assert_equal(fork1.getblockcount(), 37)

        # create new epoch on fork1 and check that finalizer votes
        #       J           v0                       v1
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7[31, 32] fork0
        #                            \                         v2                v3
        #                             - 30] - e7[...] - e8[36, 37, ...] - e9[41, 42] fork1
        generate_block(fork1, count=4)
        assert_equal(fork1.getblockcount(), 41)
        v3 = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork1)
        assert_vote(vote_raw_tx=v3, input_raw_tx=v2, source_epoch=5, target_epoch=8, target_hash=fork1.getblockhash(40))
        generate_block(fork1)
        assert_equal(fork1.getblockcount(), 42)

        # create longer fork0 and check that after reorg finalizer doesn't vote
        #       J           v0                v1
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7 - e8 - e9[41,42, 43] fork0
        #                            \             v2          v3
        #                             - 30] - e7 - e8 - e9[41, 42] fork1
        generate_block(fork0, count=11)
        assert_equal(fork0.getblockcount(), 43)
        connect_nodes(finalizer, fork0.index)
        sync_blocks([finalizer, fork0])
        assert_equal(finalizer.getblockcount(), 43)
        assert_equal(len(fork0.getrawmempool()), 0)
        disconnect_nodes(finalizer, fork0.index)
        self.log.info('finalizer successfully detected potential two consecutive double votes and did not vote')

        # check that finalizer can vote from next epoch on fork0
        #       J           v0                v1                          v4
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7 - e8 - e9[...] - e10[46, 47] fork0
        #                            \             v2          v3
        #                             - 30] - e7 - e8 - e9[41, 42] fork1
        generate_block(fork0, count=3)
        assert_equal(fork0.getblockcount(), 46)
        v4 = self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork0)
        assert_vote(vote_raw_tx=v4, input_raw_tx=v1, source_epoch=5, target_epoch=9, target_hash=fork0.getblockhash(45))
        generate_block(fork0)
        assert_equal(fork0.getblockcount(), 47)

        # finalize epoch8 on fork1 and re-broadcast all vote txs
        # which must not create slash tx
        #       J           v0                v1                                      v4
        # ... - e5 - e6[26, 27, 28, 29, 30] - e7 - e8[   ...    ] - e9[...] - e10[46, 47] fork0
        #                            \             F      v2        J      v3
        #                             - 30] - e7 - e8[36, 37,...] - e9[41, 42, 43] - e10[46, 47] fork1
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork1)
        generate_block(fork1)
        assert_equal(fork1.getblockcount(), 43)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
                                         'currentEpoch': 9,
                                         'lastJustifiedEpoch': 8,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 2})

        generate_block(fork1, count=3)
        assert_equal(fork1.getblockcount(), 46)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork1)
        generate_block(fork1)
        assert_equal(fork1.getblockcount(), 47)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
                                         'currentEpoch': 10,
                                         'lastJustifiedEpoch': 9,
                                         'lastFinalizedEpoch': 8,
                                         'validators': 2})

        assert_raises_rpc_error(-26, 'bad-vote-invalid', fork1.sendrawtransaction, v1)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', fork1.sendrawtransaction, v4)
        assert_equal(len(fork1.getrawmempool()), 0)

        assert_raises_rpc_error(-26, 'bad-vote-invalid', fork0.sendrawtransaction, v2)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', fork0.sendrawtransaction, v3)
        assert_equal(len(fork0.getrawmempool()), 0)
        self.log.info('re-broadcasting existing votes did not create slash tx')


if __name__ == '__main__':
    VoteReorgTest().main()
