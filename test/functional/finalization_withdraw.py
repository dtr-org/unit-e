#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_finalizationstate,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    json,
    sync_blocks,
    wait_until,
)
from decimal import Decimal
import time

LOGOUT_DYNASTY_DELAY = 3
WITHDRAW_EPOCH_DELAY = 12


class WithdrawTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3

        esperanza_config = {
            'dynastyLogoutDelay': LOGOUT_DYNASTY_DELAY,
            'withdrawalEpochDelay': WITHDRAW_EPOCH_DELAY
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
        assert_equal(finalizer1.getbalance(), Decimal('10000'))

        # Leave IBD
        generate_block(proposer)
        sync_blocks([proposer, finalizer1, finalizer2], timeout=10)

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
        generate_block(proposer)
        sync_blocks([proposer, finalizer1, finalizer2], timeout=10)
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
        # F    F    F    F
        # e0 - e1 - e2 - e3 - e4[16]
        #      d1
        #      d2
        generate_block(proposer, count=3 + 5 + 5)
        assert_equal(proposer.getblockcount(), 15)
        assert_finalizationstate(proposer, {'currentDynasty': 1,
                                            'currentEpoch': 3,
                                            'lastJustifiedEpoch': 2,
                                            'lastFinalizedEpoch': 2,
                                            'validators': 0})

        generate_block(proposer)
        assert_equal(proposer.getblockcount(), 16)
        assert_finalizationstate(proposer, {'currentDynasty': 2,
                                            'currentEpoch': 4,
                                            'lastJustifiedEpoch': 2,
                                            'lastFinalizedEpoch': 2,
                                            'validators': 2})
        self.log.info('finalizers are created')

        # Logout finalizer1
        # F    F    F    F
        # e0 - e1 - e2 - e3 - e4[16, 17, 18, 19, 20]
        #      d1                        l1
        #      d2
        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)

        # TODO UNIT-E: logout tx can't be created if its vote is not in the block
        # we should check that input of logout tx is in the mempool too
        generate_block(proposer)

        connect_nodes(finalizer1, proposer.index)
        sync_blocks([finalizer1, proposer], timeout=10)
        l1 = finalizer1.logout()
        wait_until(lambda: l1 in proposer.getrawmempool(), timeout=10)
        disconnect_nodes(finalizer1, proposer.index)

        generate_block(proposer, count=3)
        assert_equal(proposer.getblockcount(), 20)
        assert_finalizationstate(proposer, {'currentDynasty': 2,
                                            'currentEpoch': 4,
                                            'lastJustifiedEpoch': 3,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 2})
        self.log.info('finalizer1 logged out in dynasty=2')

        # During LOGOUT_DYNASTY_DELAY both finalizers can vote.
        # Since the finalization happens at every epoch,
        # number of dynasties is equal to number of epochs.
        for _ in range(LOGOUT_DYNASTY_DELAY):
            generate_block(proposer)
            self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
            self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
            generate_block(proposer, count=4)
            assert_raises_rpc_error(-25,
                                    "Logout delay hasn't passed yet. Can't withdraw.",
                                    finalizer1.withdraw,
                                    finalizer1_address)

        assert_equal(proposer.getblockcount(), 35)
        assert_finalizationstate(proposer, {'currentDynasty': 5,
                                            'currentEpoch': 7,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 6,
                                            'validators': 2})

        self.log.info('finalizer1 voted during logout delay successfully')

        # During WITHDRAW_DELAY finalizer1 can't vote and can't withdraw
        generate_block(proposer)
        assert_finalizationstate(proposer, {'currentDynasty': 6,
                                            'currentEpoch': 8,
                                            'lastJustifiedEpoch': 6,
                                            'lastFinalizedEpoch': 6,
                                            'validators': 1})
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
        generate_block(proposer)
        assert_finalizationstate(proposer, {'currentDynasty': 6,
                                            'currentEpoch': 8,
                                            'lastJustifiedEpoch': 7,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 1})

        # finalizer1 can't vote so we keep it connected
        connect_nodes(finalizer1, proposer.index)
        time.sleep(2)  # ensure no votes from finalizer1
        assert_equal(len(proposer.getrawmempool()), 0)

        generate_block(proposer, count=3)
        assert_equal(proposer.getblockcount(), 40)
        assert_finalizationstate(proposer, {'currentDynasty': 6,
                                            'currentEpoch': 8,
                                            'lastJustifiedEpoch': 7,
                                            'lastFinalizedEpoch': 7,
                                            'validators': 1})
        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'WAITING_FOR_WITHDRAW_DELAY')
        assert_raises_rpc_error(-25,
                                "Withdraw delay hasn't passed yet. Can't withdraw.",
                                finalizer1.withdraw,
                                finalizer1_address)

        # WITHDRAW_DELAY - 1 is because we checked the first loop manually
        for _ in range(WITHDRAW_EPOCH_DELAY - 1):
            generate_block(proposer)
            self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
            generate_block(proposer, count=4)

        assert_equal(proposer.getblockcount(), 95)
        assert_finalizationstate(proposer, {'currentDynasty': 17,
                                            'currentEpoch': 19,
                                            'lastJustifiedEpoch': 18,
                                            'lastFinalizedEpoch': 18,
                                            'validators': 1})

        # last block that finalizer1 can't withdraw
        # TODO UNIT-E: allow to create a withdraw tx on checkpoint
        # as it will be added to the block on the next epoch only.
        # We have an known issue https://github.com/dtr-org/unit-e/issues/643
        # that finalizer can't vote after checkpoint is processed, it looks that
        # finalizer can't create any finalizer commits at this point (and only at this point).
        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'WAITING_FOR_WITHDRAW_DELAY')
        assert_raises_rpc_error(-25,
                                "Withdraw delay hasn't passed yet. Can't withdraw.",
                                finalizer1.withdraw,
                                finalizer1_address)

        self.log.info('finalizer1 could not withdraw during WITHDRAW_DELAY period')

        # test that deposit can be withdrawn
        # e0 - e1 - ... - e4 - ... - e20[95, 96, 97]
        #      d1         l1                     w1
        #      d2
        generate_block(proposer)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
        assert_equal(proposer.getblockcount(), 96)
        assert_finalizationstate(proposer, {'currentDynasty': 18,
                                            'currentEpoch': 20,
                                            'lastJustifiedEpoch': 18,
                                            'lastFinalizedEpoch': 18,
                                            'validators': 1})
        sync_blocks([proposer, finalizer1], timeout=10)
        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'WAITING_TO_WITHDRAW')
        assert_equal(finalizer1.getbalance(), Decimal('9999.99993840'))
        w1 = finalizer1.withdraw(finalizer1_address)
        wait_until(lambda: w1 in proposer.getrawmempool(), timeout=10)
        generate_block(proposer)
        sync_blocks([proposer, finalizer1])
        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'NOT_VALIDATING')
        assert_equal(finalizer1.getbalance(), Decimal('9999.99992140'))

        self.log.info('finalizer1 was able to withdraw deposit at dynasty=18')

        # test that withdraw commit can be spent
        # test that deposit can be withdrawn
        # e0 - e1 - ... - e4 - ... - e20[95, 96, 97, 98]
        #      d1         l1                     w1  spent_w1
        #      d2
        spent_w1_raw = finalizer1.createrawtransaction(
            [{'txid': w1, 'vout': 0}], {finalizer1_address: Decimal('1499.999')})
        spent_w1_signed = finalizer1.signrawtransactionwithwallet(spent_w1_raw)
        spent_w1 = finalizer1.sendrawtransaction(spent_w1_signed['hex'])
        self.wait_for_transaction(spent_w1, nodes=[proposer])

        # mine block
        block_hash = generate_block(proposer)[0]
        assert spent_w1 in proposer.getblock(block_hash)['tx']

        self.log.info('finalizer1 was able to spend withdraw commit')

        # Test that after withdraw the node can deposit again
        sync_blocks([proposer, finalizer1], timeout=10)
        assert_equal(proposer.getblockcount(), 98)
        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'NOT_VALIDATING')
        deposit = finalizer1.deposit(finalizer1.getnewaddress('', 'legacy'), 1500)
        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_CONFIRMATION')

        self.wait_for_transaction(deposit, timeout=10, nodes=[proposer, finalizer1])
        proposer.generate(1)
        sync_blocks([proposer, finalizer1], timeout=10)
        assert_equal(proposer.getblockcount(), 99)

        assert_equal(finalizer1.getvalidatorinfo()['validator_status'], 'WAITING_DEPOSIT_FINALIZATION')

        self.log.info('finalizer1 deposits again')

        # Test that finalizer is voting after depositing again
        disconnect_nodes(finalizer1, proposer.index)

        proposer.generate(2)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
        assert_equal(proposer.getblockcount(), 101)

        proposer.generate(5)
        self.wait_for_vote_and_disconnect(finalizer=finalizer2, node=proposer)
        assert_equal(proposer.getblockcount(), 106)
        assert_finalizationstate(proposer, {'currentDynasty': 20,
                                            'currentEpoch': 22,
                                            'lastJustifiedEpoch': 20,
                                            'lastFinalizedEpoch': 20,
                                            'validators': 2})

        self.wait_for_vote_and_disconnect(finalizer=finalizer1, node=proposer)
        self.log.info('finalizer1 votes again')


if __name__ == '__main__':
    WithdrawTest().main()
