#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Measures propagation time of vote transactions to inbound and outbound peer and
asserts it is faster than some threshold
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_less_than,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
    sync_mempools,
    wait_until,
)
from test_framework.messages import (
    FromHex,
    CTransaction,
    TxType,
)

import time

TEST_SAMPLES = 21
VOTE_PROPAGATION_THRESHOLD_SEC = 0.5


class FeatureNoVoteTxRelayDelayTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":2}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config, '-validating=1'],
        ]

        self.num_nodes = len(self.extra_args)

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        # used to de-duplicate tx ids
        vote_tx_ids = set()

        inbound = self.nodes[0]
        middle = self.nodes[1]
        outbound = self.nodes[2]
        finalizer = self.nodes[3]

        self.setup_stake_coins(middle, finalizer)

        # create network topology where arrows denote the connection direction:
        #         finalizer
        #             ↑
        # inbound → middle → outbound
        connect_nodes(inbound, middle.index)
        connect_nodes(middle, outbound.index)
        connect_nodes(middle, finalizer.index)

        self.log.info('Topology of the network is configured')

        def mean(l):
            return sum(l) / len(l)

        def median(l):
            assert_equal(len(l) % 2, 1)
            return sorted(l)[int(len(l)/2)]

        def new_votes_in_mempool(node):
            mempool = node.getrawmempool()
            return [txid for txid in mempool if txid not in vote_tx_ids]

        def calc_vote_relay_delay(record_to):
            # UNIT-E TODO: node can't vote when it processed the checkpoint
            # so we create one extra block to pass that. See https://github.com/dtr-org/unit-e/issues/643
            generate_block(middle)

            # ensure all nodes are synced before recording the delay
            sync_blocks([middle, record_to], timeout=10)
            sync_mempools([middle, record_to], timeout=10)
            assert_equal(len(new_votes_in_mempool(middle)), 0)

            # ensure that record_from node receives the block earlier than the vote
            disconnect_nodes(middle, finalizer.index)
            generate_block(middle)
            connect_nodes(middle, finalizer.index)

            wait_until(lambda: len(new_votes_in_mempool(middle)) > 0, timeout=10)

            now = time.perf_counter()
            sync_mempools([middle, record_to], wait=0.05, timeout=10)
            delay = time.perf_counter() - now

            new_votes = new_votes_in_mempool(middle)
            assert_equal(len(new_votes), 1)
            new_vote = new_votes[0]
            vote_tx_ids.add(new_vote)

            # sanity check: tx we measured is a vote tx
            tx = FromHex(CTransaction(), middle.getrawtransaction(new_vote))
            assert_equal(tx.get_type(), TxType.VOTE)

            self.log.debug("Vote(%s) propagated from %d to %d in %0.3f seconds"
                           % (new_vote, middle.index, record_to.index, delay))

            return delay

        # leave IBD
        generate_block(middle)
        sync_blocks(self.nodes, timeout=10)

        # disable instant finalization
        payto = finalizer.getnewaddress('', 'legacy')
        txid = finalizer.deposit(payto, 1500)
        self.wait_for_transaction(txid, timeout=10)

        generate_block(middle, count=8)
        assert_equal(middle.getblockcount(), 9)
        assert_equal(middle.getfinalizationstate()['currentEpoch'], 5)
        sync_blocks(self.nodes, timeout=10)

        # record relay time of the vote transaction to the outbound peer
        outbound_vote_delays = []
        for _ in range(TEST_SAMPLES):
            delay = calc_vote_relay_delay(outbound)
            outbound_vote_delays.append(delay)

        self.log.info('Test outbound vote relay %d times. mean: %0.3f s, median:'
                      ' %0.3f s, min: %0.3f s, max: %0.3f s',
                      TEST_SAMPLES, mean(outbound_vote_delays),
                      median(outbound_vote_delays), min(outbound_vote_delays),
                      max(outbound_vote_delays))

        # record relay time of the vote transaction to the inbound peer
        inbound_vote_delays = []
        for _ in range(TEST_SAMPLES):
            delay = calc_vote_relay_delay(inbound)
            inbound_vote_delays.append(delay)

        self.log.info('Test inbound vote relay %d times. mean: %0.3f s, median: '
                      '%0.3f s, min: %0.3f s, max: %0.3f s',
                      TEST_SAMPLES, mean(inbound_vote_delays),
                      median(inbound_vote_delays), min(inbound_vote_delays),
                      max(inbound_vote_delays))

        assert_less_than(mean(outbound_vote_delays), VOTE_PROPAGATION_THRESHOLD_SEC)
        assert_less_than(mean(inbound_vote_delays), VOTE_PROPAGATION_THRESHOLD_SEC)


if __name__ == '__main__':
    FeatureNoVoteTxRelayDelayTest().main()
