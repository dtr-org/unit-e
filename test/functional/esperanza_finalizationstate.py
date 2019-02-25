#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import json
from test_framework.util import assert_equal
from test_framework.util import sync_blocks
from test_framework.util import JSONRPCException
from test_framework.util import connect_nodes_bi
from test_framework.util import disconnect_nodes
from test_framework.util import wait_until
from test_framework.test_framework import UnitETestFramework

block_time = 1


def test_setup(test, proposers, validators):

    test.num_nodes = validators + proposers
    test.extra_args = []

    params_data = {
        'epochLength': 10,
        'minDepositSize': 1500,
    }

    json_params = json.dumps(params_data)

    proposer_node_params = [
        '-stakesplitthreshold=100000000000',
        '-proposing=1',
        '-debug=all',
        '-whitelist=127.0.0.1',
        '-connect=0',
        '-listen=1',
        '-esperanzaconfig=' + json_params
    ]

    validator_node_params = [
        '-validating=1',
        '-proposing=0',
        '-whitelist=127.0.0.1',
        '-debug=all',
        '-connect=0',
        '-listen=1',
        '-esperanzaconfig=' + json_params
    ]

    for n in range(0, proposers):
        test.extra_args.append(proposer_node_params)

    for n in range(0, validators):
        test.extra_args.append(validator_node_params)

    test.setup_clean_chain = True


def setup_deposit(self, proser, validators):

    for i, n in enumerate(validators):
        n.new_address = n.getnewaddress("", "legacy")

        assert_equal(n.getbalance(), 10000)

    for n in validators:
        deptx = n.deposit(n.new_address, 1500)
        self.wait_for_transaction(deptx)

    # the validator will be ready to operate in epoch 3
    # TODO: UNIT - E: it can be 2 epochs as soon as #572 is fixed
    for n in range(0, 29):
        generate_block(proser)

    assert_equal(proser.getblockchaininfo()['blocks'], 30)


def generate_block(node):
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


# The scenario tested is the case where a vote from a validator
# makes it into a proposer mempool but at the moment of the
# proposal the vote is expired.
# The vote should not be added to a block (that would be invalid)
# but the proposer should still be able to create a block disregarding
# the expired vote.
# node[0] and node[1] are proposer (p0, p1)
# node[2] is the validator (v)
class ExpiredVoteTest(UnitETestFramework):

    def set_test_params(self):
        test_setup(self, 2, 1)

    def setup_network(self):
        self.setup_nodes()

        # create a connection v1 -> p1 <- p2
        connect_nodes_bi(self.nodes, 0, 1)
        connect_nodes_bi(self.nodes, 1, 2)

    def run_test(self):

        p0 = self.nodes[0]
        p1 = self.nodes[1]
        v = self.nodes[2]

        self.setup_stake_coins(p0, p1, v)

        # Leave IBD
        self.generate_sync(p0)

        setup_deposit(self, p0, [v])
        sync_blocks([p0, p1, v])

        # get to up to block 148, just one before the new checkpoint
        for n in range(0, 8):
            generate_block(p0)

        assert_equal(p0.getblockchaininfo()['blocks'], 38)
        sync_blocks([p0, p1, v])

        # Rearrange connection like p0 -> p1 xxx v so the validator
        # remains isolated. And the generate the new checkpoint
        disconnect_nodes(p1, 2)
        generate_block(p0)
        generate_block(p0)
        sync_blocks([p0, p1])

        # Disconnect now also p0 and reconnect p1 to v so v can catch up
        # and vote on the new epoch but the p1 and v remain segregated from
        # p0.
        disconnect_nodes(p0, 1)
        connect_nodes_bi(self.nodes, 1, 2)

        # wait for the vote to be propagated to p1
        sync_blocks([p1, v])
        wait_until(lambda: p1.getmempoolinfo()['size'] == 1, timeout=3)

        # Mine another epoch while disconnected p0.
        for n in range(0, 10):
            generate_block(p0)

        assert_equal(p0.getblockchaininfo()['blocks'], 50)

        # now we disconnect v again so it will not vote in the epoch just created
        # since that would interfere with the test.
        disconnect_nodes(p1, 2)
        connect_nodes_bi(self.nodes, 0, 1)
        sync_blocks([p0, p1])

        # Check nothing else made it to the mempool in the meanwhile
        wait_until(lambda: p1.getmempoolinfo()['size'] == 1, timeout=3)

        # now p1 should propose but the vote he has in the mempool is
        # not valid anymore. Make sure that we can still generate a block
        # even if the vote in mempool is currently invalid.
        generate_block(p1)
        sync_blocks([p0, p1])

        assert_equal(p1.getblockchaininfo()['blocks'], 51)


if __name__ == '__main__':
    ExpiredVoteTest().main()
