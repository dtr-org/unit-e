#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
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
            '-proposing=0',
            '-debug=all',
            '-rescan=1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-proposing=0', '-debug=all', '-esperanzaconfig=' + json_params]

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

        assert_equal(validator.getwalletinfo()['balance'], 10000)

        # wait for coinbase maturity
        nodes[1].generate(120)
        sync_blocks(self.nodes[0:3])

        txid = validator.createdeposit(payto, 10000)['transactionid']

        # wait for transaction to propagate
        self.wait_for_transaction(txid)

        # mine some blocks to allow the deposit to get included in a block
        for n in range(0, 10):
            self.generate_block(nodes[(n % 3) + 1])
            sync_blocks(self.nodes[0:3])
            time.sleep(block_time)

        sync_blocks(self.nodes[0:3])

        resp = validator.getvalidatorinfo()
        assert resp["enabled"]
        assert_equal(resp["validator_status"], "IS_VALIDATING")

        return

    def generate_block(self, node):
        i = 0
        # It is rare but possible that a block was valid at the moment of creation but
        # invalid at submission. This is to account for those cases.
        while i < 5:
            try :
                node.generate(1)
                break
            except:
                i += 1
                time.sleep(1)

if __name__ == '__main__':
    EsperanzaDepositTest().main()
