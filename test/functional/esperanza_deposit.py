#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from random import randint
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class EsperanzaDepositTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': 2,
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-staking=0',
            '-debug=all',
            '-rescan=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-staking=0', '-debug=all', '-esperanzaconfig=' + json_params]

        self.extra_args = [validator_node_params,
                           proposer_node_params,
                           proposer_node_params,
                           proposer_node_params]
        self.setup_clean_chain = True

    def run_test(self):
        nodes = self.nodes
        block_time = 1

        validator = nodes[0]

        validator.importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        nodes[1].importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
        nodes[2].importmasterkey('narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat')
        nodes[3].importmasterkey('soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade')

        payto = validator.getnewaddress("", "legacy")

        validator.rescanblockchain(0, 0)
        nodes[1].rescanblockchain(0, 0)
        nodes[2].rescanblockchain(0, 0)
        nodes[3].rescanblockchain(0, 0)

        # wait for the rescan to be done
        while validator.getwalletinfo()['immature_balance'] != 10000:
            time.sleep(0.5)

        nodes[1].generate(120)

        sync_blocks(self.nodes[0:3])

        txid = validator.createdeposit(payto, 10000)['transactionid']

        # wait for transaction to propagate
        self.wait_for_transaction(txid)

        # mine some blocks to allow the deposit to get included in a block
        for n in range(0, 10):
            nodes[(n % 3) + 1].generate(1)
            time.sleep(block_time)

        sync_blocks(self.nodes[0:3])

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        return

    def propose_block(self):
        selected_node = randint(0, self.num_nodes - 1)
        self.nodes[selected_node].generate(1)

    def wait_for_transaction(self, txid):
        while True:
            try:
                for n in range(0, self.num_nodes):
                    self.nodes[n].getrawtransaction(txid)
                break
            except JSONRPCException:
                continue

if __name__ == '__main__':
    EsperanzaDepositTest().main()
