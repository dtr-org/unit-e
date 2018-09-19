#!/usr/bin/env python3

from test_framework.util import *
from test_framework.test_framework import UnitETestFramework

block_time = 1
master_keys = ['swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready',
               'chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly',
               'narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat',
               'soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade']


def test_setup(test, proposers, validators):
    test.num_nodes = validators + proposers
    test.extra_args = []

    params_data = {
        'epochLength': 10,
        'minDepositSize': 1500,
    }
    json_params = json.dumps(params_data)

    validator_node_params = [
        '-validating=1',
        '-proposing=0',
        '-whitelist=127.0.0.1',
        '-debug=all',
        '-esperanzaconfig=' + json_params
    ]
    proposer_node_params = ['-proposing=0', '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig=' + json_params]

    for n in range(0, proposers):
        test.extra_args.append(proposer_node_params)

    for n in range(0, validators):
        test.extra_args.append(validator_node_params)

    test.setup_clean_chain = True


def setup_deposit(self, nodes):

    i = 0
    for n in nodes:
        n.importmasterkey(master_keys[i])
        n.new_address = n.getnewaddress("", "legacy")

        assert_equal(n.getwalletinfo()['balance'], 10000)

    # wait for coinbase maturity
    for n in range(0, 120):
        generate_block(nodes[0])
    assert (nodes[0].getblockchaininfo()['blocks'] == 120)

    sync_blocks(self.nodes[0:len(nodes)])

    for n in nodes:
        deptx = n.createdeposit(n.new_address, 1500)['transactionid']
        self.wait_for_transaction(deptx)

    # finalize deposits and start voting
    for n in range(0, 20):
        generate_block(nodes[0])

    assert (nodes[0].getblockchaininfo()['blocks'] == 140)


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
            print("error generating block: " + exp.error)
    raise AssertionError("Node" + str(node.index) + " cannot generate block")


# The scenario tested is the case where a vote from a validator
# makes it into a proposer mempool but at the moment of the
# proposal the vote is expired.
# The vote should not be added to a block (that would be invalid)
# but the proposer should still be able to create a block disregarding
# the expired vote.
# node[0] and node[1] are proposer (p1,p2)
# node[2] is the validator (v1)
class ExpiredVoteTest(UnitETestFramework):

    def set_test_params(self):
        test_setup(self, 2, 1)

    def setup_network(self):
        self.setup_nodes()

        # create a connection v0 -> p1 <- p2
        connect_nodes_bi(self.nodes, 0, 2)
        connect_nodes_bi(self.nodes, 1, 2)

    def run_test(self):
        nodes = self.nodes

        setup_deposit(self, [nodes[2]])

        # generate a votable epoch
        for n in range(0, 10):
            generate_block(nodes[0])

        assert (nodes[0].getblockchaininfo()['blocks'] == 150)

        # Disconnect immediately one proposer. A vote not yet included in blocks
        # should now reach the p2 that will accept it.
        disconnect_nodes(nodes[0], 2)
        disconnect_nodes(nodes[0], 1)

        # wait for the vote to be propagated to p2
        time.sleep(5)

        # Mine another epoch while disconnected p1.
        for n in range(0, 10):
            generate_block(nodes[0])

        assert (nodes[0].getblockchaininfo()['blocks'] == 160)

        # connect again and wait for sync
        connect_nodes_bi(self.nodes, 0, 2)
        connect_nodes_bi(self.nodes, 1, 2)
        sync_blocks(self.nodes[0:2])

        # now p2 should propose but the vote he has in the mempool is
        # not valid anymore.
        generate_block(nodes[1])
        sync_blocks(self.nodes[0:2])

        assert(nodes[0].getblockchaininfo()['blocks'] == 161)

        print("Test succeeded.")

        return


if __name__ == '__main__':
    ExpiredVoteTest().main()
