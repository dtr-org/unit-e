#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
SlashTest checks:
1. double vote with invalid vote signature is ignored
2. double vote with valid vote signature but invalid tx signature creates slash transaction
"""
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.script import CScript
from test_framework.test_framework import UnitETestFramework
from test_framework.blocktools import (
    CBlock,
    CTransaction,
    FromHex,
    ToHex,
    TxType,
    sign_transaction)
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    sync_blocks,
    wait_until,
)
import time


class SlashTest(UnitETestFramework):

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

    def test_double_votes(self):
        def corrupt_script(script, n_byte):
            script = bytearray(script)
            script[n_byte] = 1 if script[n_byte] == 0 else 0
            return bytes(script)

        # initial topology where arrows denote the direction of connections
        # finalizer2 ← fork1 → finalizer1
        #                ↓  ︎
        #              fork2
        fork1 = self.nodes[0]
        fork2 = self.nodes[1]

        fork1.importmasterkey(regtest_mnemonics[0]['mnemonics'])
        fork2.importmasterkey(regtest_mnemonics[1]['mnemonics'])

        finalizer1 = self.nodes[2]
        finalizer2 = self.nodes[3]

        connect_nodes(fork1, fork2.index)
        connect_nodes(fork1, finalizer1.index)
        connect_nodes(fork1, finalizer2.index)

        # leave IBD
        generate_block(fork1)
        sync_blocks([fork1, fork2, finalizer1, finalizer2])

        # clone finalizer
        finalizer2.importmasterkey(regtest_mnemonics[2]['mnemonics'])
        finalizer1.importmasterkey(regtest_mnemonics[2]['mnemonics'])

        disconnect_nodes(fork1, finalizer2.index)
        addr = finalizer1.getnewaddress('', 'legacy')
        txid1 = finalizer1.deposit(addr, 1500)
        wait_until(lambda: txid1 in fork1.getrawmempool())

        finalizer2.setlabel(addr, '')
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

        # test that same vote included on different forks
        # doesn't create a slash transaction
        #                                       v1
        #                          - e4[17, 18, 19, 20] fork1
        # F    F    F    F        /
        # e0 - e1 - e2 - e3 - e4[16]
        #                         \     v1
        #                          - e4[17, 18, 19, 20] fork2
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=fork1)
        v1 = fork1.getrawtransaction(fork1.getrawmempool()[0])
        generate_block(fork1, count=4)
        assert_equal(fork1.getblockcount(), 20)
        assert_finalizationstate(fork1, {'currentEpoch': 4,
                                         'lastJustifiedEpoch': 3,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})

        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork2)
        generate_block(fork2)
        assert_raises_rpc_error(-27, 'transaction already in block chain', fork2.sendrawtransaction, v1)
        assert_equal(len(fork2.getrawmempool()), 0)
        generate_block(fork2, count=3)
        assert_equal(fork2.getblockcount(), 20)
        assert_finalizationstate(fork1, {'currentEpoch': 4,
                                         'lastJustifiedEpoch': 3,
                                         'lastFinalizedEpoch': 3,
                                         'validators': 1})
        self.log.info('same vote on two forks was accepted')

        # test that double-vote with invalid vote signature is ignored
        # and doesn't cause slashing
        #                            v1          v2a
        #                          - e4 - e5[21, 22] fork1
        # F    F    F    F     F  /
        # e0 - e1 - e2 - e3 - e4[16]
        #                         \  v1          v2a
        #                          - e4 - e5[21, 22] fork2
        generate_block(fork1)
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=fork1)
        v2a = fork1.getrawtransaction(fork1.getrawmempool()[0])
        generate_block(fork1)
        assert_equal(fork1.getblockcount(), 22)
        assert_finalizationstate(fork1, {'currentEpoch': 5,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})

        generate_block(fork2)
        tx_v2a = FromHex(CTransaction(), v2a)

        # corrupt the 1st byte of the validator's pubkey in the commit script
        # see schema in CScript::CommitScript
        tx_v2a.vout[0].scriptPubKey = corrupt_script(script=tx_v2a.vout[0].scriptPubKey, n_byte=2)

        assert_raises_rpc_error(-26, 'bad-vote-signature', fork2.sendrawtransaction, ToHex(tx_v2a))
        assert_equal(len(fork2.getrawmempool()), 0)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=fork2)
        time.sleep(10)  # slash transactions are processed every 10 sec. UNIT-E TODO: remove once optimized
        assert_equal(len(fork2.getrawmempool()), 1)
        v2b = fork2.getrawtransaction(fork2.getrawmempool()[0])
        tx_v2b = FromHex(CTransaction(), v2b)
        assert_equal(tx_v2b.get_type(), TxType.VOTE)

        generate_block(fork2)
        assert_equal(len(fork2.getrawmempool()), 0)
        assert_equal(fork2.getblockcount(), 22)
        assert_finalizationstate(fork1, {'currentEpoch': 5,
                                         'lastJustifiedEpoch': 4,
                                         'lastFinalizedEpoch': 4,
                                         'validators': 1})
        self.log.info('double-vote with invalid signature is ignored')

        # test that valid double-vote but corrupt withdraw address
        # creates slash tx it is included in the next block
        #                            v1          v2a
        #                          - e4 - e5[21, 22] fork1
        # F    F    F    F     F  /
        # e0 - e1 - e2 - e3 - e4[16]
        #                         \  v1          v2a s1
        #                          - e4 - e5[21, 22, 23] fork2
        # corrupt the 1st byte of the address in the scriptpubkey
        # but keep the correct vote signature see schema in CScript::CommitScript
        tx_v2a = FromHex(CTransaction(), v2a)

        # Remove the signature
        tx_v2a.vin[0].scriptSig = list(CScript(tx_v2a.vin[0].scriptSig))[1]
        tx_v2a.vout[0].scriptPubKey = corrupt_script(script=tx_v2a.vout[0].scriptPubKey, n_byte=42)
        tx_v2a = sign_transaction(finalizer2, tx_v2a)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', fork2.sendrawtransaction, ToHex(tx_v2a))
        wait_until(lambda: len(fork2.getrawmempool()) == 1, timeout=20)
        s1_hash = fork2.getrawmempool()[0]
        s1 = FromHex(CTransaction(), fork2.getrawtransaction(s1_hash))
        assert_equal(s1.get_type(), TxType.SLASH)

        b23 = generate_block(fork2)[0]
        block = FromHex(CBlock(), fork2.getblock(b23, 0))
        assert_equal(len(block.vtx), 2)
        block.vtx[1].rehash()
        assert_equal(block.vtx[1].hash, s1_hash)
        self.log.info('slash tx for double-vote was successfully created')

    def run_test(self):
        self.test_double_votes()


if __name__ == '__main__':
    SlashTest().main()
