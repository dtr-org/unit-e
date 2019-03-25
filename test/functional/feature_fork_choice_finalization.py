#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
ForkChoiceFinalizationTest checks:
1. node always follows the longest justified fork
2. node doesn't switch to heavier but less justified fork
3. node switches to the heaviest fork with the same justification
"""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    connect_nodes,
    assert_finalizationstate,
    disconnect_nodes,
    assert_equal,
    sync_blocks,
    sync_chain,
    wait_until,
    JSONRPCException,
)


class ForkChoiceFinalizationTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 8
        self.setup_clean_chain = True

        esperanza_config = '-esperanzaconfig={"epochLength":5}'
        self.extra_args = [
            # test_justification_over_chain_work
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config, '-validating=1'],

            # test_longer_justification
            [esperanza_config],
            [esperanza_config],
            [esperanza_config],
            [esperanza_config, '-validating=1'],
        ]

    def setup_network(self):
        self.setup_nodes()

    @staticmethod
    def have_tx_in_mempool(nodes, txid):
        for n in nodes:
            if txid not in n.getrawmempool():
                return False
        return True

    def test_justification_over_chain_work(self):
        """
        Test that justification has priority over chain work
        """

        def seen_block(node, blockhash):
            try:
                node.getblock(blockhash)
                return True
            except JSONRPCException:
                return False

        def connect_sync_disconnect(node1, node2, blockhash):
            connect_nodes(node1, node2.index)
            wait_until(lambda: seen_block(node1, blockhash), timeout=10)
            wait_until(lambda: node1.getblockcount() == node2.getblockcount(), timeout=5)
            assert_equal(node1.getblockhash(node1.getblockcount()), blockhash)
            disconnect_nodes(node1, node2.index)

        node0 = self.nodes[0]
        node1 = self.nodes[1]
        node2 = self.nodes[2]
        validator = self.nodes[3]

        self.setup_stake_coins(node0, node1, node2, validator)

        connect_nodes(node0, node1.index)
        connect_nodes(node0, node2.index)
        connect_nodes(node0, validator.index)

        # leave IBD
        node0.generatetoaddress(1, node0.getnewaddress('', 'bech32'))
        sync_blocks([node0, node1, node2, validator])

        payto = validator.getnewaddress('', 'legacy')
        txid = validator.deposit(payto, 1500)
        wait_until(lambda: self.have_tx_in_mempool([node0, node1, node2], txid))

        disconnect_nodes(node0, node1.index)
        disconnect_nodes(node0, node2.index)
        disconnect_nodes(node0, validator.index)
        assert_equal(len(node0.getpeerinfo()), 0)

        # 0 ... 4 ... 9 ... 14 ... 19 ... 24 ... 29 - 30
        #       F     F     F      F      J          tip
        node0.generatetoaddress(29, node0.getnewaddress('', 'bech32'))
        assert_equal(node0.getblockcount(), 30)
        assert_finalizationstate(node0, {'currentDynasty': 4,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        connect_nodes(node0, node1.index)
        connect_nodes(node0, node2.index)
        sync_blocks([node0, node1, node2])
        disconnect_nodes(node0, node1.index)
        disconnect_nodes(node0, node2.index)

        # generate fork with no commits. node0 must switch to it
        # 30 node1
        #   \
        #    - b31 node0, node2
        b31 = node2.generatetoaddress(1, node2.getnewaddress('', 'bech32'))[-1]
        connect_sync_disconnect(node0, node2, b31)
        assert_equal(node0.getblockcount(), 31)

        # generate fork with justified commits. node0 must switch to it
        #    - 31 - b32 node0, node1
        #   /
        # 30
        #   \
        #    - b31 node2
        connect_nodes(node1, validator.index)
        sync_blocks([node1, validator])
        wait_until(lambda: len(node1.getrawmempool()) > 0, timeout=150)
        b32 = node1.generatetoaddress(2, node1.getnewaddress('', 'bech32'))[-1]
        sync_blocks([node1, validator])
        disconnect_nodes(node1, validator.index)
        connect_sync_disconnect(node0, node1, b32)
        assert_finalizationstate(node0, {'currentDynasty': 4,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})
        self.log.info('node successfully switched to longest justified fork')

        # generate longer but not justified fork. node0 shouldn't switch
        #    - 31 - b32 node0, node1, node2
        #   /
        # 30
        #   \
        #    - 31 - 32 - 33 - b34
        b34 = node2.generatetoaddress(3, node2.getnewaddress('', 'bech32'))[-1]
        assert_equal(node2.getblockcount(), 34)
        assert_equal(node0.getblockcount(), 32)

        connect_nodes(node0, node2.index)
        sync_chain([node0, node2])
        sync_blocks([node0, node2])

        assert_equal(node0.getblockcount(), 32)
        assert_equal(node0.getblockhash(32), b32)
        assert_equal(node0.getfinalizationstate()['lastJustifiedEpoch'], 5)
        self.log.info('node did not switch to heaviest but less justified fork')

        assert_equal(node2.getblockcount(), 32)
        assert_equal(node2.getblockhash(32), b32)
        assert_equal(node2.getfinalizationstate()['lastJustifiedEpoch'], 5)
        self.log.info('node switched to logest justified fork with less work')

        self.stop_node(node0.index)
        self.stop_node(node1.index)
        self.stop_node(node2.index)
        self.stop_node(validator.index)

    def test_heaviest_justified_epoch(self):
        """
        Test that heaviest justified epoch wins
        """
        fork1 = self.nodes[4]
        fork2 = self.nodes[5]
        fork3 = self.nodes[6]
        finalizer = self.nodes[7]

        self.setup_stake_coins(fork1, fork2, fork3, finalizer)

        connect_nodes(fork1, fork2.index)
        connect_nodes(fork1, fork3.index)
        connect_nodes(fork1, finalizer.index)

        # leave IBD
        fork1.generatetoaddress(1, fork1.getnewaddress('', 'bech32'))
        sync_blocks([fork1, fork2, finalizer])

        # add deposit
        payto = finalizer.getnewaddress('', 'legacy')
        txid = finalizer.deposit(payto, 1500)
        wait_until(lambda: self.have_tx_in_mempool([fork1, fork2], txid))

        # leave instant justification
        fork1.generatetoaddress(3 + 5 + 5 + 5 + 5 + 1, fork1.getnewaddress('', 'bech32'))
        assert_equal(fork1.getblockcount(), 25)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 5,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        wait_until(lambda: len(fork1.getrawmempool()) == 1, timeout=10)
        fork1.generatetoaddress(4, fork1.getnewaddress('', 'bech32'))
        assert_equal(fork1.getblockcount(), 29)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 5,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 3})

        # justify epoch=5
        # J
        # e5 - e6 fork1, fork2, fork3
        fork1.generatetoaddress(1, fork1.getnewaddress('', 'bech32'))
        wait_until(lambda: len(fork1.getrawmempool()) == 1, timeout=10)
        fork1.generatetoaddress(4, fork1.getnewaddress('', 'bech32'))
        assert_equal(fork1.getblockcount(), 34)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4})

        # create two forks at epoch=6 that use the same votes to justify epoch=5
        #             fork3
        # F     J     |
        # e5 - e6[.., 34] - e7[35, 36] fork1
        #                       \
        #                        - 36, 37] fork2
        sync_blocks([fork1, fork3])
        disconnect_nodes(fork1, fork3.index)
        fork1.generatetoaddress(1, fork1.getnewaddress('', 'bech32'))
        sync_blocks([fork1, fork2])

        for fork in [fork1, fork2]:
            wait_until(lambda: len(fork.getrawmempool()) == 1, timeout=10)
            assert_equal(fork.getblockcount(), 35)
            assert_finalizationstate(fork, {'currentDynasty': 5,
                                            'currentEpoch': 7,
                                            'lastJustifiedEpoch': 5,
                                            'lastFinalizedEpoch': 4})

        disconnect_nodes(fork1, fork2.index)
        vote = fork1.getrawtransaction(fork1.getrawmempool()[0])

        for fork in [fork1, fork2]:
            fork.generatetoaddress(1, fork.getnewaddress('', 'bech32'))
            assert_equal(fork.getblockcount(), 36)
            assert_finalizationstate(fork, {'currentDynasty': 5,
                                            'currentEpoch': 7,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 5})

        b37 = fork2.generatetoaddress(1, fork2.getnewaddress('', 'bech32'))[0]

        # test that fork1 switches to the heaviest fork
        #             fork3
        # F     J     |
        # e5 - e6[.., 34] - e7[35, 36] fork1
        #                       \
        #                        - 36, 37] fork2, fork1
        connect_nodes(fork1, fork2.index)
        fork1.waitforblock(b37)

        assert_equal(fork1.getblockcount(), 37)
        assert_equal(fork1.getblockhash(37), b37)
        assert_finalizationstate(fork1, {'currentDynasty': 5,
                                         'currentEpoch': 7,
                                         'lastJustifiedEpoch': 6,
                                         'lastFinalizedEpoch': 5})

        disconnect_nodes(fork1, fork2.index)

        # test that fork1 switches to the heaviest fork
        #                 - e7[35, 36, 37, 38, 39] fork3, fork1
        # F     J       /
        # e5 - e6[.., 34] - e7[35, 36]
        #                       \
        #                        - 36, 37] fork2, fork1
        assert_equal(fork3.getblockcount(), 34)
        fork3.generatetoaddress(4, fork3.getnewaddress('', 'bech32'))
        fork3.sendrawtransaction(vote)
        wait_until(lambda: len(fork3.getrawmempool()) == 1, timeout=10)
        b39 = fork3.generatetoaddress(1, fork3.getnewaddress('', 'bech32'))[0]
        assert_equal(fork3.getblockcount(), 39)

        connect_nodes(fork1, fork3.index)
        fork1.waitforblock(b39)

        assert_equal(fork1.getblockcount(), 39)
        assert_equal(fork1.getblockhash(39), b39)
        assert_finalizationstate(fork1, {'currentDynasty': 5,
                                         'currentEpoch': 7,
                                         'lastJustifiedEpoch': 6,
                                         'lastFinalizedEpoch': 5})

        self.stop_node(fork1.index)
        self.stop_node(fork2.index)
        self.stop_node(fork3.index)
        self.stop_node(finalizer.index)

    def run_test(self):
        self.log.info("start test_justification_over_chain_work")
        self.test_justification_over_chain_work()
        self.log.info("test_justification_over_chain_work passed")

        self.log.info("start test_heaviest_justified_epoch")
        self.test_heaviest_justified_epoch()
        self.log.info("test_heaviest_justified_epoch passed")


if __name__ == '__main__':
    ForkChoiceFinalizationTest().main()
