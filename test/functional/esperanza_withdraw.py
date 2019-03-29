#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    json,
    connect_nodes,
    disconnect_nodes,
    assert_equal,
    assert_finalizationstate,
    assert_raises_rpc_error,
    sync_blocks,
    wait_until,
)
from decimal import Decimal
import time

LOGOUT_DELAY = 3
WITHDRAW_DELAY = 12


class EsperanzaWithdrawTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3

        esperanza_config = {
            'dynastyLogoutDelay': LOGOUT_DELAY,
            'withdrawalEpochDelay': WITHDRAW_DELAY
        }
        json_params = json.dumps(esperanza_config)

        finalizer_node_params = ['-esperanzaconfig=' + json_params, '-validating=1']
        proposer_node_params = ['-esperanzaconfig=' + json_params]

        self.extra_args = [
            proposer_node_params,
            finalizer_node_params,
            finalizer_node_params,
        ]
        self.setup_clean_chain = True

    # create topology where arrows denote non-persistent connection
    # finalizer1 → proposer ← finalizer2
    def setup_network(self):
        self.setup_nodes()

        proposer = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]

        connect_nodes(finalizer1, proposer.index)
        connect_nodes(finalizer2, proposer.index)

    def run_test(self):
        proposer = self.nodes[0]
        finalizer1 = self.nodes[1]
        finalizer2 = self.nodes[2]

        self.setup_stake_coins(*self.nodes)

        # Leave IBD
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
        sync_blocks([proposer, finalizer1, finalizer2])

        finalizer1_address = finalizer1.getnewaddress('', 'legacy')

        # create deposits
        # F
        # e0 - e1
        #      d1
        #      d2
        d1 = finalizer1.deposit(finalizer1_address, 1500)
        d2 = finalizer2.deposit(finalizer2.getnewaddress('', 'legacy'), 1500)
        self.wait_for_transaction(d1, timeout=10)
        self.wait_for_transaction(d2, timeout=10)
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
        sync_blocks([proposer, finalizer1, finalizer2])
        disconnect_nodes(finalizer1, proposer.index)
        disconnect_nodes(finalizer2, proposer.index)
        assert_equal(proposer.getblockcount(), 2)
        assert_finalizationstate(proposer, {'currentDynasty': 0,
                                            'currentEpoch': 1,
                                            'lastJustifiedEpoch': 0,
                                            'lastFinalizedEpoch': 0,
                                            'validators': 0})
        self.log.info('deposits are created')

        # Generate enough blocks to activate deposits
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6[26]
        #      d1
        #      d2
        proposer.generatetoaddress(3 + 5 + 5 + 5 + 5, proposer.getnewaddress('', 'bech32'))
        assert_equal(proposer.getblockcount(), 25)
        assert_finalizationstate(proposer, {'currentDynasty': 2,
                                            'currentEpoch': 5,
                                            'lastJustifiedEpoch': 4,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 0})

        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
        assert_equal(proposer.getblockcount(), 26)
        assert_finalizationstate(proposer, {'currentDynasty': 3,
                                            'currentEpoch': 6,
                                            'lastJustifiedEpoch': 4,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 2})
        self.log.info('finalizers are created')

        # Logout finalizer1
        # F    F    F    F    J
        # e0 - e1 - e2 - e3 - e4 - e5 - e6[26]
        #      d1                       l1
        #      d2
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)

        # TODO UNIT-E: logout tx can't be created if its vote is not in the block
        # we should check that input of logout tx is in the mempool too
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))

        connect_nodes(finalizer1, proposer.index)
        l1 = finalizer1.logout()
        wait_until(lambda: l1 in proposer.getrawmempool(), timeout=10)
        disconnect_nodes(finalizer1, proposer.index)

        proposer.generatetoaddress(3, proposer.getnewaddress('', 'bech32'))
        assert_equal(proposer.getblockcount(), 30)
        assert_finalizationstate(proposer, {'currentDynasty': 3,
                                            'currentEpoch': 6,
                                            'lastJustifiedEpoch': 5,
                                            'lastFinalizedEpoch': 4,
                                            'validators': 2})
        self.log.info('finalizer1 logged out in dynasty=3')

        # During LOGOUT_DELAY both finalizers can vote.
        # LOGOUT_DELAY - 1 is because finalizer1 is already voted
        # in the same dynasty it logged out
        for _ in range(LOGOUT_DELAY - 1):
            proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
            self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
            self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
            proposer.generatetoaddress(4, proposer.getnewaddress('', 'bech32'))

        assert_equal(proposer.getblockcount(), 40)
        assert_finalizationstate(proposer, {'currentDynasty': 5,
                                            'currentEpoch': 8,
                                            'lastJustifiedEpoch': 7,
                                            'lastFinalizedEpoch': 6,
                                            'validators': 2})
        self.log.info('finalizer1 voted during logout delay successfully')

        # During WITHDRAW_DELAY finalizer1 can't vote and can't withdraw
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
        assert_finalizationstate(proposer, {'currentDynasty': 6,
                                            'currentEpoch': 9,
                                            'lastJustifiedEpoch': 7,
                                            'lastFinalizedEpoch': 6,
                                            'validators': 1})
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))

        # finalizer1 can't vote so we keep it connected
        connect_nodes(finalizer1, proposer.index)
        time.sleep(2)  # ensure no votes from finalizer1
        assert_equal(len(proposer.getrawmempool()), 0)

        proposer.generatetoaddress(3, proposer.getnewaddress('', 'bech32'))
        assert_equal(proposer.getblockcount(), 45)
        assert_finalizationstate(proposer, {'currentDynasty': 6,
                                            'currentEpoch': 9,
                                            'lastJustifiedEpoch': 8,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 1})

        # WITHDRAW_DELAY - 1 is because we checked the first loop manually
        for _ in range(WITHDRAW_DELAY - 1):
            proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
            self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
            proposer.generatetoaddress(4, proposer.getnewaddress('', 'bech32'))

        assert_equal(proposer.getblockcount(), 100)
        assert_finalizationstate(proposer, {'currentDynasty': 17,
                                            'currentEpoch': 20,
                                            'lastJustifiedEpoch': 19,
                                            'lastFinalizedEpoch': 18,
                                            'validators': 1})

        # last block that finalizer1 can't withdraw
        # TODO UNIT-E: actually finalizer shouldn't withdraw on checkout -1
        # but after the checkpoint is processed it's OK to create withdraw commit
        # as it will be added to the block of the next epoch only.
        # We have an known issue https://github.com/dtr-org/unit-e/issues/643
        # that finalizer can't vote after checkpoint is processed, it looks that
        # finalizer can't create any finalizer commits at this point (and only at this point).
        assert_raises_rpc_error(-8, 'Cannot send withdraw transaction.', finalizer1.withdraw, finalizer1_address)

        self.log.info('finalizer1 could not withdraw during WITHDRAW_DELAY period')

        # test that deposit can be withdrawn
        # e0 - e1 - ... - e6 - ... - e21[101, 102]
        #      d1         l1                  w1
        #      d2
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
        assert_equal(proposer.getblockcount(), 101)
        assert_finalizationstate(proposer, {'currentDynasty': 18,
                                            'currentEpoch': 21,
                                            'lastJustifiedEpoch': 19,
                                            'lastFinalizedEpoch': 18,
                                            'validators': 1})
        w1 = finalizer1.withdraw(finalizer1_address)
        wait_until(lambda: w1 in proposer.getrawmempool(), timeout=10)
        proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))

        self.log.info('finalizer1 was able to withdraw deposit at dynasty=18')

        # test that withdraw commit can be spent
        # test that deposit can be withdrawn
        # e0 - e1 - ... - e6 - ... - e21[101, 102, 103]
        #      d1         l1                  w1   spent_w1
        #      d2
        spent_w1_raw = finalizer1.createrawtransaction(
            [{'txid': w1, 'vout': 0}], {finalizer1_address: Decimal('1499.999')})
        spent_w1_signed = finalizer1.signrawtransaction(spent_w1_raw)
        spent_w1 = finalizer1.sendrawtransaction(spent_w1_signed['hex'])
        wait_until(lambda: spent_w1 in proposer.getrawmempool(), timeout=10)

        # mine block
        block_hash = proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))[0]
        assert spent_w1 in proposer.getblock(block_hash)['tx']

        self.log.info('finalizer1 was able to spend withdraw commit')


if __name__ == '__main__':
    EsperanzaWithdrawTest().main()
