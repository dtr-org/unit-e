#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test fork choice rule between parallel justification forks

The following checks are performed:
1. node re-orgs to the longer justified parallel fork
2. node re-orgs to the previous fork that became longer justified one
3. node doesn't re-org before finalization
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import (
    P2PInterface,
    network_thread_start,
    msg_witness_block,
)

from test_framework.util import (
    connect_nodes,
    assert_finalizationstate,
    disconnect_nodes,
    assert_equal,
    sync_blocks,
    wait_until,
)
from test_framework.messages import (
    CTransaction,
    CBlock,
    FromHex,
)
from test_framework.regtest_mnemonics import regtest_mnemonics


class BaseNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.rejects = []

    def on_reject(self, msg):
        self.rejects.append(msg)

    def has_reject(self, err, block):
        for r in self.rejects:
            if r.reason == err and r.data == block:
                return True
        return False


class ForkChoiceParallelJustificationsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 5
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config],
            ['-proposing=0', esperanza_config, '-validating=1'],
            ['-proposing=0', esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        def create_justification(fork, finalizer, after_blocks):
            fork.generatetoaddress(after_blocks - 1, fork.getnewaddress('', 'bech32'))
            self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork)
            fork.generatetoaddress(1, fork.getnewaddress('', 'bech32'))
            assert_equal(len(fork.getrawmempool()), 0)

        def sync_node_to_fork(node, fork):
            connect_nodes(node, fork.index)
            block_hash = fork.getblockhash(fork.getblockcount())
            node.waitforblock(block_hash, 5000)
            assert_equal(node.getblockhash(node.getblockcount()), block_hash)
            disconnect_nodes(node, fork.index)

        def wait_for_reject(p2p, err, block):
            wait_until(lambda: p2p.has_reject(err, block), timeout=5)

        # Two validators (but actually having the same key) produce parallel justifications
        # node must always follow the longest justified fork
        # finalizer1 -> fork1
        #             /
        #           node
        #             \
        # finalizer2 -> fork2
        node = self.nodes[0]
        fork1 = self.nodes[1]
        fork2 = self.nodes[2]
        finalizer1 = self.nodes[3]
        finalizer2 = self.nodes[4]

        node.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        finalizer1.importmasterkey(regtest_mnemonics[1]['mnemonics'])
        finalizer2.importmasterkey(regtest_mnemonics[1]['mnemonics'])
        fork1.importmasterkey(regtest_mnemonics[2]['mnemonics'])
        fork2.importmasterkey(regtest_mnemonics[2]['mnemonics'])

        # create network topology
        connect_nodes(node, fork1.index)
        connect_nodes(node, fork2.index)
        connect_nodes(finalizer1, fork1.index)
        connect_nodes(finalizer2, fork2.index)

        # leave IBD
        node.generatetoaddress(2, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork1, fork2, finalizer1, finalizer2])

        # Do not let finalizer2 to see deposit from finalizer1
        disconnect_nodes(node, fork2.index)

        payto = finalizer1.getnewaddress('', 'legacy')
        txid1 = finalizer1.deposit(payto, 1500)
        finalizer2.setaccount(payto, '')
        txid2 = finalizer2.deposit(payto, 1500)
        if txid1 != txid2:  # improve log message
            tx1 = FromHex(CTransaction(), finalizer1.getrawtransaction(txid1))
            tx2 = FromHex(CTransaction(), finalizer2.getrawtransaction(txid2))
            print(tx1)
            print(tx2)
            assert_equal(txid1, txid2)

        # Connect back
        connect_nodes(node, fork2.index)

        self.wait_for_transaction(txid1, timeout=150)

        node.generatetoaddress(1, node.getnewaddress('', 'bech32'))
        sync_blocks([node, fork1, fork2])

        disconnect_nodes(node, fork1.index)
        disconnect_nodes(node, fork2.index)
        disconnect_nodes(finalizer1, fork1.index)
        disconnect_nodes(finalizer2, fork2.index)

        # create common 5 epochs to leave instant finalization
        #                             fork1
        # F    F    F    F    J      /
        # e0 - e1 - e2 - e3 - e4 - e5 node
        #                            \
        #                             fork2
        node.generatetoaddress(22, node.getnewaddress('', 'bech32'))
        assert_equal(node.getblockcount(), 25)
        assert_finalizationstate(node, {'currentDynasty': 2,
                                        'currentEpoch': 5,
                                        'lastJustifiedEpoch': 4,
                                        'lastFinalizedEpoch': 3,
                                        'validators': 0})

        connect_nodes(node, fork1.index)
        connect_nodes(node, fork2.index)
        sync_blocks([node, fork1, fork2])
        disconnect_nodes(node, fork1.index)
        disconnect_nodes(node, fork2.index)

        # create fist justified epoch on fork1
        # node must follow this fork
        #
        #                             - e6 fork1, node
        # F    F    F    F    J    * /
        # e0 - e1 - e2 - e3 - e4 - e5
        #                            \
        #                             fork2
        # e4 is finalized for fork1
        # e5 is justified for fork1
        create_justification(fork=fork1, finalizer=finalizer1, after_blocks=2)
        assert_equal(fork1.getblockcount(), 27)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        sync_node_to_fork(node, fork1)

        assert_finalizationstate(node, {'currentDynasty': 3,
                                        'currentEpoch': 6,
                                        'lastJustifiedEpoch': 5,
                                        'lastFinalizedEpoch': 4,
                                        'validators': 1})

        self.log.info('node successfully switched to the justified fork')

        # create longer justified epoch on fork2
        # node must switch ("zig") to this fork
        #
        #                             - e6 fork1
        # F    F    F    F    F    J /
        # e0 - e1 - e2 - e3 - e4 - e5
        #                            \       J
        #                             - e6 - e7 - e8 fork2, node
        create_justification(fork=fork2, finalizer=finalizer2, after_blocks=2)
        assert_equal(fork2.getblockcount(), 27)
        assert_finalizationstate(fork2, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        create_justification(fork=fork2, finalizer=finalizer2, after_blocks=10)
        assert_equal(fork2.getblockcount(), 37)
        assert_finalizationstate(fork2, {'currentDynasty': 4,
                                         'currentEpoch': 8,
                                         'lastJustifiedEpoch': 7,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        sync_node_to_fork(node, fork2)

        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 8,
                                        'lastJustifiedEpoch': 7,
                                        'lastFinalizedEpoch': 4,
                                        'validators': 1})

        self.log.info('node successfully switched to the longest justified fork')

        # create longer justified epoch on the previous fork1
        # node must switch ("zag") to this fork
        #                                         J
        #                             - e6 - e7 - e8 - e9 fork1, node
        # F    F    F    F    F    J /
        # e0 - e1 - e2 - e3 - e4 - e5
        #                            \       J
        #                             - e6 - e7 - e8 fork2
        create_justification(fork=fork1, finalizer=finalizer1, after_blocks=16)
        assert_equal(fork1.getblockcount(), 43)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
                                         'currentEpoch': 9,
                                         'lastJustifiedEpoch': 8,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        sync_node_to_fork(node, fork1)

        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 9,
                                        'lastJustifiedEpoch': 8,
                                        'lastFinalizedEpoch': 4,
                                        'validators': 1})

        self.log.info('node successfully switched back to the longest justified fork')

        # test that re-org before finalization is not possible
        #                                         J               J*
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork1
        # F    F    F    F    F    J /                                      |
        # e0 - e1 - e2 - e3 - e4 - e5                                       56] node
        #                            \       J
        #                             - e6 - e7 - e8 fork2
        # e11 is not justified for node
        known_fork1_height = fork1.getblockcount()
        assert_equal(node.getblockcount(), known_fork1_height)

        known_fork1_hash = fork1.getblockhash(known_fork1_height)
        assert_equal(node.getblockhash(known_fork1_height), known_fork1_hash)
        create_justification(fork=fork1, finalizer=finalizer1, after_blocks=14)

        assert_equal(fork1.getblockcount(), 57)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
                                         'currentEpoch': 12,
                                         'lastJustifiedEpoch': 11,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        attacker = node.add_p2p_connection(BaseNode())
        network_thread_start()
        attacker.wait_for_verack()

        # send blocks without the last one that has a justified vote
        node_blocks = node.getblockcount()
        for h in range(known_fork1_height + 1, fork1.getblockcount()):
            block_hash = fork1.getblockhash(h)
            block = FromHex(CBlock(), fork1.getblock(block_hash, 0))
            attacker.send_message(msg_witness_block(block))
            node_blocks += 1
            wait_until(lambda: node.getblockcount() == node_blocks, timeout=15)

        assert_equal(node.getblockcount(), 56)
        assert_finalizationstate(node, {'currentDynasty': 4,
                                        'currentEpoch': 12,
                                        'lastJustifiedEpoch': 8,
                                        'lastFinalizedEpoch': 4,
                                        'validators': 1})

        # create finalization
        #                                         J               J
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork1
        # F    F    F    F    F    J /                                      |
        # e0 - e1 - e2 - e3 - e4 - e5                                       56] node
        #                            \       J         F    J
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork2
        create_justification(fork=fork2, finalizer=finalizer2, after_blocks=11)
        assert_equal(fork2.getblockcount(), 48)
        assert_finalizationstate(fork2, {'currentDynasty': 4,
                                         'currentEpoch': 10,
                                         'lastJustifiedEpoch': 9,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        create_justification(fork=fork2, finalizer=finalizer2, after_blocks=6)
        assert_equal(fork2.getblockcount(), 54)
        assert_finalizationstate(fork2, {'currentDynasty': 4,
                                         'currentEpoch': 11,
                                         'lastJustifiedEpoch': 10,
                                         'lastFinalizedEpoch': 9,
                                         'validators': 1})

        fork2.generatetoaddress(3, fork2.getnewaddress('', 'bech32'))
        assert_equal(fork2.getblockcount(), 57)
        assert_finalizationstate(fork2, {'currentDynasty': 5,
                                         'currentEpoch': 12,
                                         'lastJustifiedEpoch': 10,
                                         'lastFinalizedEpoch': 9,
                                         'validators': 1})

        # node follows longer finalization
        #                                         J               J
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork1
        # F    F    F    F    F    J /
        # e0 - e1 - e2 - e3 - e4 - e5
        #                            \       J         F    J
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork2, node
        tip = fork2.getblockhash(57)
        sync_node_to_fork(node, fork2)

        assert_equal(node.getblockcount(), 57)
        assert_finalizationstate(node, {'currentDynasty': 5,
                                        'currentEpoch': 12,
                                        'lastJustifiedEpoch': 10,
                                        'lastFinalizedEpoch': 9,
                                        'validators': 1})

        # send block with surrounded vote that justifies longer fork
        # node's view:
        #                                         J               J
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork1
        # F    F    F    F    F    J /
        # e0 - e1 - e2 - e3 - e4 - e5
        #                            \       J         F    J
        #                             - e6 - e7 - e8 - e9 - e10 - e11 - e12[56, 57] fork2, node

        block_hash = fork1.getblockhash(fork1.getblockcount())
        block = FromHex(CBlock(), fork1.getblock(block_hash, 0))
        block.calc_sha256()
        attacker.send_message(msg_witness_block(block))

        # node should't re-org to malicious fork
        wait_for_reject(attacker, b'bad-fork-dynasty', block.sha256)
        assert_equal(node.getblockcount(), 57)
        assert_equal(node.getblockhash(57), tip)
        assert_finalizationstate(node, {'currentDynasty': 5,
                                        'currentEpoch': 12,
                                        'lastJustifiedEpoch': 10,
                                        'lastFinalizedEpoch': 9,
                                        'validators': 1})

        self.log.info('node did not re-org before finalization')


if __name__ == '__main__':
    ForkChoiceParallelJustificationsTest().main()
