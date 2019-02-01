#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that esperanza transactions are relied without a delay"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    sync_mempools,
    connect_nodes,
    connect_nodes_bi,
    sync_blocks,
    wait_until,
    JSONRPCException,
)
from test_framework.messages import (
    FromHex,
    CTransaction,
)
from test_framework.admin import Admin

import time
from functools import reduce


class FeatureNoEsperanzaTxRelayDelayTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 6
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":1}'
        self.extra_args = [
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config, '-validating=1'],
            [esperanza_config, '-validating=1'],
        ]

    def run_test(self):
        # used to de-duplicate tx ids
        vote_tx_ids = dict()

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

        def calc_tx_relay_delay(generate_node, record_from, record_to):
            txid = generate_node.sendtoaddress(generate_node.getnewaddress(), 1)
            wait_until(lambda: has_tx_in_mempool(record_from, txid), timeout=40)

            now = time.perf_counter()
            wait_until(lambda: has_tx_in_mempool(record_to, txid), timeout=40)
            return time.perf_counter() - now

        def calc_vote_relay_delay(generate_node, record_from, record_to):
            # ensure all nodes are sync before recording the delay
            sync_blocks(self.nodes)
            sync_mempools(self.nodes)

            generate_node.generatetoaddress(1, generate_node.getnewaddress())
            wait_until(lambda: len(record_from.getrawmempool()) > 0, timeout=30)

            now = time.perf_counter()

            vote_tx = None
            for txid in record_from.getrawmempool():
                if txid not in vote_tx_ids:
                    vote_tx = txid
                    vote_tx_ids[txid] = True
                    break
            assert vote_tx is not None
            sync_mempools([record_from, record_to], wait=0.05, timeout=30)
            delay = time.perf_counter() - now

            # sanity check
            tx = FromHex(CTransaction(), record_from.getrawtransaction(vote_tx))
            assert_equal(tx.nVersion >> 16, 3)

            return delay

        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node2 = self.nodes[2]
        node3 = self.nodes[3]

        validator1 = self.nodes[4]
        validator2 = self.nodes[5]

        node0.importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        node2.importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
        validator1.importmasterkey('narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat')
        validator2.importmasterkey('soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade')

        # leave IBD
        node0.generatetoaddress(1, node0.getnewaddress())

        Admin.authorize_and_disable(self, node0)

        # create network topology where arrows denote the connection direction:
        # validator1 <-> node0 -> node1 -> node2 <-> validator2 <- node3
        self.restart_node(node0.index)
        self.restart_node(node1.index)
        self.restart_node(node2.index)
        self.restart_node(node3.index)
        self.restart_node(validator1.index)
        self.restart_node(validator2.index)
        connect_nodes_bi(self.nodes, validator1.index, node0.index)
        connect_nodes(node0, node1.index)
        connect_nodes(node1, node2.index)
        connect_nodes_bi(self.nodes, validator2.index, node2.index)
        connect_nodes(node3, validator2.index)

        self.log.info('Topology of the network is configured')

        # record relay time of the standard transaction to the outbound peer
        outbound_delays = []
        for i in range(5):
            delay = calc_tx_relay_delay(generate_node=node0, record_from=node1, record_to=node2)
            outbound_delays.append(delay)

        self.log.info('Outbound tx relay time. mean: %0.2f sec, median: %0.2f sec',
                      mean(outbound_delays), median(outbound_delays))

        sync_mempools(self.nodes)

        # record relay time of the standard transaction to the inbound peer
        inbound_delays = []
        for i in range(5):
            delay = calc_tx_relay_delay(generate_node=node2, record_from=node1, record_to=node0)
            inbound_delays.append(delay)

        self.log.info('Inbound tx relay time. mean: %0.3f sec, median: %0.3f sec',
                      mean(inbound_delays), median(inbound_delays))

        sync_mempools(self.nodes)

        # disable instant finalization
        for validator in [validator1, validator2]:
            payto = validator.getnewaddress('', 'legacy')
            txid = validator.deposit(payto, 10000)
            self.wait_for_transaction(txid)

        node0.generatetoaddress(2, node0.getnewaddress())
        sync_blocks(self.nodes)

        # record relay time of the vote transaction to the outbound peer
        outbound_vote_delays = []
        for i in range(5):
            delay = calc_vote_relay_delay(generate_node=node0, record_from=node0, record_to=node1)
            outbound_vote_delays.append(delay)

        self.log.info('Outbound vote relay time. mean: %0.3f sec, median: %0.3f sec',
                      mean(outbound_vote_delays), median(outbound_vote_delays))

        # record relay time of the vote transaction to the inbound peer
        inbound_vote_delays = []
        for i in range(5):
            delay = calc_vote_relay_delay(generate_node=node3, record_from=node2, record_to=node1)
            inbound_vote_delays.append(delay)

        self.log.info('Inbound vote relay time. mean: %0.3f sec, median: %0.3f sec',
                      mean(inbound_vote_delays), median(inbound_vote_delays))

        assert mean(inbound_vote_delays) < mean(inbound_delays) / 2
        assert median(inbound_vote_delays) < median(inbound_delays) / 2
        assert mean(outbound_vote_delays) < mean(outbound_delays) / 2
        assert median(outbound_vote_delays) < mean(outbound_delays) / 2


if __name__ == '__main__':
    FeatureNoEsperanzaTxRelayDelayTest().main()
