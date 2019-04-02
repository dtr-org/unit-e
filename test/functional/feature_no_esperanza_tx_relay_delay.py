#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
FeatureNoEsperanzaTxRelayDelayTest does the following:
1. measures time propagation of standard transactions to inbound and outbound peer
2. measures time propagation of vote transactions to inbound and outbound peer
3. tests that vote propagation is significantly faster than propagating standard transactions
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    sync_mempools,
    connect_nodes,
    connect_nodes_bi,
    disconnect_nodes,
    sync_blocks,
    wait_until,
    JSONRPCException,
)
from test_framework.messages import (
    FromHex,
    CTransaction,
    TxType,
)

import time
from functools import reduce

TEST_SAMPLES = 11


class FeatureNoEsperanzaTxRelayDelayTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":2}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config, '-stakesplitthreshold=5000000000'],
            [esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        # used to de-duplicate tx ids
        vote_tx_ids = set()

        def has_tx_in_mempool(node, txid):
            try:
                node.getmempoolentry(txid)
                return True
            except JSONRPCException:
                return False

        def mean(l):
            return reduce((lambda a, b: a + b), l) / len(l)

        def median(l):
            return sorted(l)[int(len(l)/2)]

        def new_votes_in_mempool(node):
            mempool = node.getrawmempool()
            return [txid for txid in mempool if txid not in vote_tx_ids]

        def calc_tx_relay_delay(generate_node, record_from, record_to):
            txid = generate_node.sendtoaddress(generate_node.getnewaddress('', 'bech32'), 1)
            wait_until(lambda: has_tx_in_mempool(record_from, txid), timeout=150)

            now = time.perf_counter()
            wait_until(lambda: has_tx_in_mempool(record_to, txid), timeout=150)
            delay = time.perf_counter() - now

            self.log.info("Tx(%s) propagated from %d to %d in %0.2f seconds" % (txid, record_from.index, record_to.index, delay))

            return delay

        def calc_vote_relay_delay(generate_node, record_from, record_to, finalizer):
            # UNIT-E TODO: node can't vote when it processed the checkpoint
            # so we create one extra block to pass that. See https://github.com/dtr-org/unit-e/issues/643
            generate_node.generatetoaddress(1, generate_node.getnewaddress('', 'bech32'))

            # ensure all nodes are synced before recording the delay
            sync_blocks([generate_node, record_from, record_to], timeout=10)
            sync_mempools([generate_node, record_from, record_to], timeout=10)
            assert_equal(len(new_votes_in_mempool(record_from)), 0)

            # ensure that record_from node receives the block earlier than the vote
            disconnect_nodes(generate_node, finalizer.index)
            generate_node.generatetoaddress(1, generate_node.getnewaddress('', 'bech32'))
            sync_blocks([generate_node, record_from], timeout=10)
            connect_nodes(generate_node, finalizer.index)

            wait_until(lambda: len(new_votes_in_mempool(record_from)) > 0, timeout=10)

            now = time.perf_counter()

            vote_tx = None
            for txid in record_from.getrawmempool():
                if txid not in vote_tx_ids:
                    vote_tx = txid
                    vote_tx_ids.add(vote_tx)
                    break
            assert vote_tx is not None
            sync_mempools([record_from, record_to], wait=0.05, timeout=10)
            delay = time.perf_counter() - now

            # sanity check: tx we measured is a vote tx
            tx = FromHex(CTransaction(), record_from.getrawtransaction(vote_tx))
            assert_equal(tx.get_type(), TxType.VOTE.name)

            self.log.info("Vote(%s) propagated from %d to %d in %0.2f seconds" % (vote_tx, record_from.index, record_to.index, delay))

            return delay

        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node2 = self.nodes[2]
        node3 = self.nodes[3]

        finalizer = self.nodes[4]

        self.setup_stake_coins(node3, finalizer)

        # create network topology where arrows denote the connection direction:
        #         node3 → finalizer
        #          ↓↑
        # node0 → node1 → node2
        connect_nodes(node0, node1.index)
        connect_nodes(node1, node2.index)
        connect_nodes(node3, finalizer.index)

        connect_nodes_bi(self.nodes, node3.index, node1.index)

        self.log.info('Topology of the network is configured')

        # leave IBD
        node3.generatetoaddress(1, node3.getnewaddress('', 'bech32'))
        sync_blocks(self.nodes, timeout=10)

        # record relay time of the standard transaction to the outbound peer
        outbound_delays = []
        for _ in range(TEST_SAMPLES):
            delay = calc_tx_relay_delay(generate_node=node3, record_from=node1, record_to=node2)
            outbound_delays.append(delay)

        self.log.info('Test outbound tx relay %d times. mean: %0.2f sec, median: %0.2f sec',
                      TEST_SAMPLES, mean(outbound_delays), median(outbound_delays))

        sync_mempools(self.nodes)

        # record relay time of the standard transaction to the inbound peer
        inbound_delays = []
        for _ in range(TEST_SAMPLES):
            delay = calc_tx_relay_delay(generate_node=node3, record_from=node1, record_to=node0)
            inbound_delays.append(delay)

        self.log.info('Test inbound tx relay %d times. mean: %0.3f sec, median: %0.3f sec',
                      TEST_SAMPLES, mean(inbound_delays), median(inbound_delays))

        sync_mempools(self.nodes)

        # disable instant finalization
        payto = finalizer.getnewaddress('', 'legacy')
        txid = finalizer.deposit(payto, 1500)
        self.wait_for_transaction(txid, timeout=10)

        node3.generatetoaddress(6, node3.getnewaddress('', 'bech32'))
        assert_equal(node3.getblockcount(), 7)
        assert_equal(node3.getfinalizationstate()['currentEpoch'], 4)
        sync_blocks(self.nodes, timeout=10)

        # record relay time of the vote transaction to the outbound peer
        outbound_vote_delays = []
        for _ in range(TEST_SAMPLES):
            delay = calc_vote_relay_delay(generate_node=node3, record_from=node1, record_to=node2, finalizer=finalizer)
            outbound_vote_delays.append(delay)

        self.log.info('Test outbound vote relay %d times. mean: %0.3f sec, median: %0.3f sec',
                      TEST_SAMPLES, mean(outbound_vote_delays), median(outbound_vote_delays))

        # record relay time of the vote transaction to the inbound peer
        inbound_vote_delays = []
        for _ in range(TEST_SAMPLES):
            delay = calc_vote_relay_delay(generate_node=node3, record_from=node1, record_to=node0, finalizer=finalizer)
            inbound_vote_delays.append(delay)

        self.log.info('Test inbound vote relay %d times. mean: %0.3f sec, median: %0.3f sec',
                      TEST_SAMPLES, mean(inbound_vote_delays), median(inbound_vote_delays))

        assert mean(inbound_vote_delays) < mean(inbound_delays)
        assert median(inbound_vote_delays) < median(inbound_delays)
        assert mean(outbound_vote_delays) < mean(outbound_delays)
        assert median(outbound_vote_delays) < mean(outbound_delays)

        assert False


if __name__ == '__main__':
    FeatureNoEsperanzaTxRelayDelayTest().main()
