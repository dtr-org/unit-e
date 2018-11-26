#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import sync_blocks
from test_framework.util import time
from test_framework.util import JSONRPCException
from test_framework.test_framework import UnitETestFramework
from test_framework.admin import Admin


class EsperanzaVoteTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 4

        params_data = {
            'epochLength': 10,
            'minDepositSize': 1500,
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-proposing=0',
            '-debug=all',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-proposing=0','-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig='+json_params]

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

        assert(all(nodes[i].getbalance() == 10000 for i in range(0, 4)))

        # wait for coinbase maturity
        for n in range(0, 119):
            self.generate_block(nodes[0])

        sync_blocks(self.nodes)

        # generates 1 more block
        Admin.authorize_and_disable(self, nodes[0])

        deptx1 = nodes[1].deposit(address1, 1500)
        deptx2 = nodes[2].deposit(address2, 2000)
        deptx3 = nodes[3].deposit(address3, 1500)

        self.wait_for_transaction(deptx1)
        self.wait_for_transaction(deptx2)
        self.wait_for_transaction(deptx3)

        # After we generated the first 120 blocks with no validators the state is
        # - currentEpoch: 12 (we are in the first block of this epoch)
        # - currentDynasty: 11
        # - lastFinalizedEpoch: 11
        # - lastJustifiedEpoch: 11
        # - validators: 0
        # Then we generate other 10 epochs
        for n in range(0, 50):
            self.generate_block(nodes[0])
            sync_blocks(self.nodes)
            time.sleep(block_time)

        resp = nodes[0].getfinalizationstate()
        assert_equal(resp["currentEpoch"], 17)
        assert_equal(resp["currentDynasty"], 16)
        assert_equal(resp["lastFinalizedEpoch"], 15)
        assert_equal(resp["lastJustifiedEpoch"], 16)
        assert_equal(resp["validators"], 3)


    def generate_block(self, node):
        i = 0
        # It is rare but possible that a block was valid at the moment of creation but
        # invalid at submission. This is to account for those cases.
        while i < 5:
            try:
                node.generate(1)
                return
            except JSONRPCException as exp:
                i += 1
                print("error generating block:", exp.error)
        raise AssertionError("Node" + str(node.index) + " cannot generate block")

if __name__ == '__main__':
    EsperanzaVoteTest().main()
