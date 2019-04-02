#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import os
import shutil

from test_framework.util import (
    json,
    assert_finalizationstate,
    sync_blocks,
    assert_equal,
    assert_raises_rpc_error,
    assert_less_than,
    wait_until,
    connect_nodes,
    disconnect_nodes,
)
from test_framework.test_framework import UnitETestFramework
from test_framework.messages import (
    FromHex,
    CTransaction,
    TxType,
)


class EsperanzaDepositTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 2

        params_data = {
            'epochLength': 10,
        }
        json_params = json.dumps(params_data)

        finalizer_node_params = [
            '-validating=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-esperanzaconfig=' + json_params]

        self.extra_args = [proposer_node_params,
                           finalizer_node_params]
        self.setup_clean_chain = True

    # create topology where arrows denote non-persistent connection
    # finalizer → proposer
    def setup_network(self):
        self.setup_nodes()

        proposer = self.nodes[0]
        finalizer = self.nodes[1]

        connect_nodes(finalizer, proposer.index)

    def run_test(self):
        proposer = self.nodes[0]
        finalizer = self.nodes[1]

        self.setup_stake_coins(proposer, finalizer)
        assert_equal(finalizer.getbalance(), finalizer.initial_stake)

        # Leave IBD
        proposer.generate(1)
        sync_blocks([proposer, finalizer])

        test_not_enough_money_for_deposit(finalizer)
        test_deposit_too_small(finalizer)
        self.test_successful_deposit(finalizer, proposer)
        test_duplicate_deposit(finalizer)
        self.test_restart_finalizer(finalizer, proposer)

    def test_successful_deposit(self, finalizer, proposer):

        payto = finalizer.getnewaddress("", "legacy")
        txid = finalizer.deposit(payto, 1500)

        deposit_tx = finalizer.gettransaction(txid)
        assert_equal(deposit_tx['amount'], 0)  # 0 because we send the money to ourselves
        assert_less_than(deposit_tx['fee'], 0)  # fee returned by gettransaction is negative

        raw_deposit_tx = finalizer.decoderawtransaction(deposit_tx['hex'])
        assert_equal(raw_deposit_tx['vout'][0]['value'], 1500)
        assert_equal(raw_deposit_tx['vout'][1]['value'], 10000 - 1500 + deposit_tx['fee'])

        # wait for transaction to propagate
        self.wait_for_transaction(txid, 10)

        wait_until(lambda: finalizer.getvalidatorinfo()['validator_status'] == 'WAITING_DEPOSIT_CONFIRMATION',
                   timeout=5)

        # mine a block to allow the deposit to get included
        self.generate_sync(proposer)
        disconnect_nodes(finalizer, proposer.index)

        wait_until(lambda: finalizer.getvalidatorinfo()['validator_status'] == 'WAITING_DEPOSIT_FINALIZATION',
                   timeout=5)

        # move to checkpoint
        proposer.generate(8)
        assert_equal(proposer.getblockcount(), 10)
        assert_finalizationstate(proposer, {'currentEpoch': 1,
                                            'currentDynasty': 0,
                                            'lastJustifiedEpoch': 0,
                                            'lastFinalizedEpoch': 0,
                                            'validators': 0})

        # the finalizer will be ready to operate at currentDynasty=3
        for _ in range(4):
            proposer.generate(10)
            assert_finalizationstate(proposer, {'validators': 0})

        # start new dynasty
        proposer.generate(1)
        assert_equal(proposer.getblockcount(), 51)
        assert_finalizationstate(proposer, {'currentEpoch': 6,
                                            'currentDynasty': 3,
                                            'lastJustifiedEpoch': 4,
                                            'lastFinalizedEpoch': 3,
                                            'validators': 1})

        connect_nodes(finalizer, proposer.index)
        sync_blocks([finalizer, proposer], timeout=10)
        wait_until(lambda: finalizer.getvalidatorinfo()['enabled'] == 1, timeout=5)
        assert_equal(finalizer.getvalidatorinfo()['validator_status'], 'IS_VALIDATING')

        # creates actual vote
        wait_until(lambda: len(proposer.getrawmempool()) == 1, timeout=5)
        txraw = proposer.getrawtransaction(proposer.getrawmempool()[0])
        vote = FromHex(CTransaction(), txraw)
        assert_equal(vote.get_type(), TxType.VOTE)

    def test_restart_finalizer(self, finalizer, proposer):
        self.stop_node(finalizer.index)

        def clean_up(dirs, files):
            for dir in dirs:
                shutil.rmtree(get_datadir_path(self.options.tmpdir, finalizer.index) + "/regtest/" + dir)
                os.makedirs(get_datadir_path(self.options.tmpdir, finalizer.index) + "/regtest/" + dir)

            for file in files:
                os.remove(get_datadir_path(self.options.tmpdir, finalizer.index) + "/regtest/" + file)

        clean_up(['wallets', 'chainstate', 'blocks', 'snapshots', 'finalization'], ['mempool.dat'])
        new_args = self.extra_args[finalizer.index].append('-reindex')

        # Restart the node
        self.start_node(finalizer.index, new_args)
        finalizer.importmasterkey(finalizer.mnemonics, "", True)
        connect_nodes(finalizer, proposer.index)
        sync_blocks([proposer, finalizer])

        wait_until(lambda: finalizer.getvalidatorinfo()['validator_status'] == 'IS_VALIDATING',
                   timeout=5)


# Deposit all you got, not enough coins left for the fees
def test_not_enough_money_for_deposit(finalizer):
    payto = finalizer.getnewaddress("", "legacy")
    assert_raises_rpc_error(-25, "Cannot create deposit.", finalizer.deposit, payto, finalizer.initial_stake)


# Deposit less then the minimum
def test_deposit_too_small(finalizer):
    payto = finalizer.getnewaddress("", "legacy")
    assert_raises_rpc_error(-8, "Amount is below minimum allowed.", finalizer.deposit, payto, 100)


# Deposit again
def test_duplicate_deposit(finalizer):
    payto = finalizer.getnewaddress("", "legacy")
    assert_raises_rpc_error(-8, "The node is already validating.", finalizer.deposit, payto, 1500)


if __name__ == '__main__':
    EsperanzaDepositTest().main()
