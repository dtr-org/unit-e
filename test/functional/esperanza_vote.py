#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import time
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class EsperanzaTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': 10,
            'minDepositSize': 1500,
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-staking=0',
            '-debug=all',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-staking=0','-debug=all', '-esperanzaconfig='+json_params]

        self.extra_args = [proposer_node_params,
                           validator_node_params,
                           validator_node_params,
                           validator_node_params,
                           ]
        self.setup_clean_chain = True

    def run_test(self):

        block_time = 1

        nodes = self.nodes

        nodes[0].importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        nodes[1].importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
        nodes[2].importmasterkey('narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat')
        nodes[3].importmasterkey('soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade')

        address1 = nodes[1].getnewaddress("", "legacy")
        address2 = nodes[2].getnewaddress("", "legacy")
        address3 = nodes[3].getnewaddress("", "legacy")

        nodes[0].rescanblockchain(0, 0)
        nodes[1].rescanblockchain(0, 0)
        nodes[2].rescanblockchain(0, 0)
        nodes[3].rescanblockchain(0, 0)

        # wait for the rescan to be done
        while nodes[1].getwalletinfo()['immature_balance'] != 10000:
            time.sleep(0.5)

        # wait for coinbase maturity
        nodes[0].generate(120)
        sync_blocks(self.nodes[0:3])

        deptx1 = nodes[1].createdeposit(address1, 1500)['transactionid']
        deptx2 = nodes[2].createdeposit(address2, 2000)['transactionid']
        deptx3 = nodes[3].createdeposit(address3, 1500)['transactionid']

        self.wait_for_transaction(deptx1)
        self.wait_for_transaction(deptx2)
        self.wait_for_transaction(deptx3)

        # After we generated the first 120 blocks with no validators the state is
        # - currentEpoch: 24 (we are in the first block of this epoch)
        # - currentDynasty: 23
        # - lastFinalizedEpoch: 23
        # - lastJustifiedEpoch: 23
        # - validators: 0
        # Then we generate other 10 epochs
        for n in range(0, 50):
            nodes[0].generate(1)
            time.sleep(block_time)

        sync_blocks(self.nodes[0:3])

        resp = nodes[0].getesperanzastate()
        assert_equal(resp["currentEpoch"], 17)
        assert_equal(resp["currentDynasty"], 16)
        assert_equal(resp["lastFinalizedEpoch"], 15)
        assert_equal(resp["lastJustifiedEpoch"], 16)
        assert_equal(resp["validators"], 3)

        print("Test succeeded.")

        return

if __name__ == '__main__':
    EsperanzaTest().main()
