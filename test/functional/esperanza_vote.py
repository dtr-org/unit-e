#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import JSONRPCException
from test_framework.test_framework import UnitETestFramework


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
            '-debug=all',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig='+json_params]

        self.extra_args = [proposer_node_params,
                           validator_node_params,
                           validator_node_params,
                           validator_node_params,
                           ]
        self.setup_clean_chain = True

    def run_test(self):
        nodes = self.nodes

        self.setup_stake_coins(*self.nodes)

        address1 = nodes[1].getnewaddress("", "legacy")
        address2 = nodes[2].getnewaddress("", "legacy")
        address3 = nodes[3].getnewaddress("", "legacy")

        assert(all(nodes[i].getbalance() == 10000 for i in range(0, 4)))

        # Leave IBD
        self.generate_block(nodes[0])

        deptx1 = nodes[1].deposit(address1, 1500)
        deptx2 = nodes[2].deposit(address2, 2000)
        deptx3 = nodes[3].deposit(address3, 1500)

        self.wait_for_transaction(deptx1, 60)
        self.wait_for_transaction(deptx2, 60)
        self.wait_for_transaction(deptx3, 60)

        # After we generated the first block with no validators the state is
        # - currentEpoch: 0 (we are in the first block of this epoch)
        # - currentDynasty: 0
        # - lastFinalizedEpoch: 0
        # - lastJustifiedEpoch: 0
        # - validators: 0
        # Then we generate other 10 epochs
        for n in range(0, 50):
            self.generate_block(nodes[0])

        resp = nodes[0].getfinalizationstate()
        assert_equal(resp["currentEpoch"], 5)
        assert_equal(resp["currentDynasty"], 3)
        assert_equal(resp["lastFinalizedEpoch"], 3)
        assert_equal(resp["lastJustifiedEpoch"], 4)
        assert_equal(resp["validators"], 3)


    def generate_block(self, node):
        i = 0
        # It is rare but possible that a block was valid at the moment of creation but
        # invalid at submission. This is to account for those cases.
        while i < 5:
            try:
                self.generate_sync(node)
                return
            except JSONRPCException as exp:
                i += 1
                print("error generating block:", exp.error)
        raise AssertionError("Node" + str(node.index) + " cannot generate block")

if __name__ == '__main__':
    EsperanzaVoteTest().main()
