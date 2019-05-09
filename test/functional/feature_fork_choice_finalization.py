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
    assert_equal,
    assert_finalizationstate,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
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
        generate_block(node0)
        sync_blocks([node0, node1, node2, validator], timeout=10)

        payto = validator.getnewaddress('', 'legacy')
        txid = validator.deposit(payto, 1500)
        wait_until(lambda: self.have_tx_in_mempool([node0, node1, node2], txid), timeout=10)

        disconnect_nodes(node0, node1.index)
        disconnect_nodes(node0, node2.index)
        disconnect_nodes(node0, validator.index)
        assert_equal(len(node0.getpeerinfo()), 0)

        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6[26]
        generate_block(node0, count=25)
        assert_equal(node0.getblockcount(), 26)
        assert_finalizationstate(node0, {'currentDynasty': 3,
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
        # 26 node1
        #   \
        #    - b27 node0, node2
        b27 = generate_block(node2)[-1]
        connect_sync_disconnect(node0, node2, b27)
        assert_equal(node0.getblockcount(), 27)

        # generate fork with justified commits. node0 must switch to it
        #    - 27 - b28 node0, node1
        #   /
        # 26
        #   \
        #    - b27 node2
        self.wait_for_vote_and_disconnect(finalizer=validator, node=node1)
        b28 = generate_block(node1, count=2)[-1]
        connect_sync_disconnect(node0, node1, b28)
        assert_equal(node0.getblockcount(), 28)
        assert_finalizationstate(node0, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})
        self.log.info('node successfully switched to longest justified fork')

        # generate longer but not justified fork. node0 shouldn't switch
        #    - 27 - b28 node0, node1, node2
        #   /
        # 26
        #   \
        #    - 27 - 28 - 29 - b30
        generate_block(node2, count=3)[-1]  # b30
        assert_equal(node2.getblockcount(), 30)
        assert_equal(node0.getblockcount(), 28)

        connect_nodes(node0, node2.index)
        sync_blocks([node0, node2], timeout=10)

        assert_equal(node0.getblockcount(), 28)
        assert_equal(node0.getblockhash(28), b28)
        assert_equal(node0.getfinalizationstate()['lastJustifiedEpoch'], 5)
        self.log.info('node did not switch to heaviest but less justified fork')

        assert_equal(node2.getblockcount(), 28)
        assert_equal(node2.getblockhash(28), b28)
        assert_equal(node2.getfinalizationstate()['lastJustifiedEpoch'], 5)
        self.log.info('node switched to longest justified fork with less work')

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
        generate_block(fork1)
        sync_blocks([fork1, fork2, finalizer], timeout=10)

        # add deposit
        payto = finalizer.getnewaddress('', 'legacy')
        txid = finalizer.deposit(payto, 1500)
        wait_until(lambda: self.have_tx_in_mempool([fork1, fork2], txid), timeout=10)
        generate_block(fork1)
        sync_blocks([fork1, fork2, finalizer], timeout=10)
        disconnect_nodes(fork1, finalizer.index)

        # leave instant justification
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6[26]
        generate_block(fork1, count=3 + 5 + 5 + 5 + 5 + 1)
        assert_equal(fork1.getblockcount(), 26)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        # justify epoch=5
        # J
        # e5 - e6 fork1, fork2, fork3
        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork1)
        generate_block(fork1, count=4)
        assert_equal(fork1.getblockcount(), 30)
        assert_finalizationstate(fork1, {'currentDynasty': 3,
                                         'currentEpoch': 6,
                                         'lastJustifiedEpoch': 5,
                                         'lastFinalizedEpoch': 4})

        # create two forks at epoch=6 that use the same votes to justify epoch=5
        #             fork3
        # F     J     |
        # e5 - e6[.., 30] - e7[31, 32] fork1
        #                       \
        #                        - 32, 33] fork2
        sync_blocks([fork1, fork3], timeout=10)
        disconnect_nodes(fork1, fork3.index)
        generate_block(fork1)
        sync_blocks([fork1, fork2], timeout=10)

        self.wait_for_vote_and_disconnect(finalizer=finalizer, node=fork1)
        for fork in [fork1, fork2]:
            wait_until(lambda: len(fork.getrawmempool()) == 1, timeout=10)
            assert_equal(fork.getblockcount(), 31)
            assert_finalizationstate(fork, {'currentDynasty': 4,
                                            'currentEpoch': 7,
                                            'lastJustifiedEpoch': 5,
                                            'lastFinalizedEpoch': 4})

        disconnect_nodes(fork1, fork2.index)
        vote = fork1.getrawtransaction(fork1.getrawmempool()[0])

        for fork in [fork1, fork2]:
            generate_block(fork)
            assert_equal(fork.getblockcount(), 32)
            assert_finalizationstate(fork, {'currentDynasty': 4,
                                            'currentEpoch': 7,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 5})

        b33 = generate_block(fork2)[0]

        # test that fork1 switches to the heaviest fork
        #             fork3
        # F     J     |
        # e5 - e6[.., 30] - e7[31, 32]
        #                       \
        #                        - 32, 33] fork2, fork1
        connect_nodes(fork1, fork2.index)
        fork1.waitforblock(b33)

        assert_equal(fork1.getblockcount(), 33)
        assert_equal(fork1.getblockhash(33), b33)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
                                         'currentEpoch': 7,
                                         'lastJustifiedEpoch': 6,
                                         'lastFinalizedEpoch': 5})

        disconnect_nodes(fork1, fork2.index)

        # test that fork1 switches to the heaviest fork
        #                 - e7[31, 32, 33, 34, 35] fork3, fork1
        # F     J       /
        # e5 - e6[.., 30] - e7[31, 32]
        #                       \
        #                        - 32, 33] fork2
        assert_equal(fork3.getblockcount(), 30)
        generate_block(fork3, count=4)
        fork3.sendrawtransaction(vote)
        wait_until(lambda: len(fork3.getrawmempool()) == 1, timeout=10)
        b35 = generate_block(fork3)[0]
        assert_equal(fork3.getblockcount(), 35)

        connect_nodes(fork1, fork3.index)
        fork1.waitforblock(b35)

        assert_equal(fork1.getblockcount(), 35)
        assert_equal(fork1.getblockhash(35), b35)
        assert_finalizationstate(fork1, {'currentDynasty': 4,
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
