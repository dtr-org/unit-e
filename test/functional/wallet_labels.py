#!/usr/bin/env python3
# Copyright (c) 2016-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test label RPCs.

RPCs tested are:
    - getaddressesbylabel
    - listaddressgroupings
    - setlabel
"""
from collections import defaultdict
from decimal import Decimal

from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    connect_nodes_bi,
    sync_blocks,
    sync_mempools
)

class WalletLabelsTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 4
        self.extra_args = [['-mocktime=1500000000']] * 4
        self.extra_args[0] += ['-deprecatedrpc=accounts']

    # UNIT-E TODO [0.18.0]: Deleted
    def setup_network(self):
        """Don't connect nodes."""
        self.setup_nodes()

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        """Run the test twice - once using the accounts API and once using the labels API."""
        # Set up stake coins manually, to avoid key collisions
        self.nodes[2].importmasterkey(regtest_mnemonics[5]['mnemonics'])
        self.nodes[3].importmasterkey(regtest_mnemonics[6]['mnemonics'])

        # UNIT-E TODO [0.18.0]: Accounts API is deleted
        # self.log.info("Test accounts API")
        # self._run_subtest(node_index=0, some_other_node_index=2)
        self.log.info("Test labels API")
        self._run_subtest(node_index=1, some_other_node_index=3)

    def _run_subtest(self, node_index, some_other_node_index):
        node = self.nodes[node_index]
        some_other_node = self.nodes[some_other_node_index]
        # Check that there's no spendable UTXO in any wallet on the node
        assert_equal(len(node.listunspent()), 0)
        # Unlock initial fund just for this node
        self.setup_stake_coins(node)
        # Check that there's exactly one spendable UTXO in this node's wallet now
        assert_equal(len(node.listunspent()), 1)
        # The unlocked funds are 10000
        initial_balance = 10000
        assert_equal(node.getbalance(), initial_balance)

        self.log.info('- check listaddressgroupings')
        address_b = node.getnewaddress(label='', address_type='bech32')
        address_c = node.getnewaddress(label='', address_type='bech32')
        # Note each time we call generate, all generated coins go into
        # the same address, so we call twice to get two addresses w/50 each
        node.generatetoaddress(nblocks=1, address=address_b)
        # slip a hundred blocks in here to make the one generated above
        # and the 101th (first in this series actually) have a mature reward.
        node.generatetoaddress(nblocks=101, address=address_c)
        reward_for_two_mature_blocks = 3.75 * 2
        assert_equal(node.getbalance(), initial_balance + reward_for_two_mature_blocks)

        # there should be 3 address groups
        # (a) one address which is the staking address which the initial funds were sent to.
        #     this address does no longer have any funds as they were used for staking and
        #     were sent to address_b.
        # (b) one address which the reward from the first call to generatetoaddress was sent to
        #     it should carry a balance of 3.75 as the stake should have moved on to address C.
        # (c) one address which the reward from the block at height=2 was sent to (and all
        #     subsequent ones, but they are not mature yet at this point in time). It should
        #     also carry the current stake, so 10003.75 in total.
        address_groups = node.listaddressgroupings()
        assert_equal(len(address_groups), 3)
        linked_addresses = set()
        for address_group in address_groups:
            # the addresses aren't linked now, so every address group should carry one address only.
            assert_equal(len(address_group), 1)
            linked_addresses.add(address_group[0][0])
        group_a = list(filter(lambda g: g[0][0] != address_b and g[0][0] != address_c, address_groups))
        group_b = list(filter(lambda g: g[0][0] == address_b, address_groups))
        group_c = list(filter(lambda g: g[0][0] == address_c, address_groups))
        # there should be only one group for each address
        assert_equal(1, len(group_a))
        assert_equal(1, len(group_b))
        assert_equal(1, len(group_c))
        # and each group should hold just one address
        assert_equal(1, len(group_a[0]))
        assert_equal(1, len(group_b[0]))
        assert_equal(1, len(group_c[0]))
        # with the aforementioned balances
        assert_equal(group_a[0][0][1], 0)
        assert_equal(group_b[0][0][1], 3.75)
        assert_equal(group_c[0][0][1], 10003.75)

        # connect that other node
        connect_nodes_bi(self.nodes, node_index, some_other_node_index)
        sync_blocks([node, some_other_node])

        # send everything (0 + 3.75 + 10003.75 = 10007.5)
        common_address = some_other_node.getnewaddress('', 'bech32')
        txid = node.sendmany(
            fromaccount="",
            amounts={common_address: 10007.5},
            subtractfeefrom=[common_address],
            minconf=1,
        )
        # this transaction should hold the whole balance of this wallet give or take some fees.
        tx_details = node.gettransaction(txid)
        fee = -tx_details['fee']
        # when we sent money we sent from address_b and address_c only as address_a did have
        # a balance of zero. Hence address_b and address_c should be joined in an address group
        # now, and address_a should remain in it's own address group. A fourth address, part of
        # the same group as address_a and address_b should have the fees.
        address_groups = node.listaddressgroupings()
        # This makes a total of two groups.
        assert_equal(len(address_groups), 2)
        # address_a will be on it's own now
        group_a = list(filter(lambda g: g[0][0] != address_b and g[0][0] != address_c, address_groups))
        # there should be just one group with address_a
        assert_equal(len(group_a), 1)
        # that group should have just that one address
        assert_equal(len(group_a[0]), 1)
        group_bc = list(filter(lambda g: g[0][0] == address_b or g[0][0] == address_c, address_groups))
        # there should be just one group holding address_b and address_c
        assert_equal(len(group_bc), 1)
        # that group should hold two addresses
        assert_equal(len(group_bc[0]), 2)
        # which should be address_b and address_c
        assert_equal(set(map(lambda g: g[0], group_bc[0])), set([address_b, address_c]))
        # the total balance should be zero as we sent everything to the common_address
        # at some_other_node
        assert_equal(node.getbalance(), 0)

        # the other node should have funds to generate a block
        sync_mempools([node, some_other_node])
        some_other_node.generatetoaddress(1, common_address)
        sync_blocks([node, some_other_node])

        assert_equal(some_other_node.getbalance(), Decimal(20007.5) - fee)

        # make funds from some_other_node available to node
        node.importprivkey(some_other_node.dumpprivkey(common_address))

        # we want to reset so that the "" label has what's expected.
        # otherwise we're off by exactly the fee amount as that's mined
        # and matures in the next 100 blocks
        amount_to_send = 1.0

        self.log.info('- Create labels and make sure subsequent label API calls')
        # recognize the label/address associations.
        labels = [Label(name) for name in ("a", "b", "c", "d", "e")]
        for label in labels:
            address = node.getnewaddress(label.name)
            label.add_receive_address(address)
            label.verify(node)

        self.log.info('- Check all labels are returned by listlabels.')
        assert_equal(list(filter(lambda l: l, node.listlabels())), [label.name for label in labels])

        # Send a transaction to each label.
        for label in labels:
            node.sendtoaddress(label.addresses[0], amount_to_send)
            label.verify(node)

        self.log.info('- Check the amounts received.')
        node.generate(1)
        for label in labels:
            assert_equal(
                node.getreceivedbyaddress(label.addresses[0]), amount_to_send)
            assert_equal(node.getreceivedbylabel(label.name), amount_to_send)

        for i, label in enumerate(labels):
            to_label = labels[(i + 1) % len(labels)]
            node.sendtoaddress(to_label.addresses[0], amount_to_send)
        node.generate(1)
        for label in labels:
            address = node.getnewaddress(label.name)
            label.add_receive_address(address)
            label.verify(node)
            assert_equal(node.getreceivedbylabel(label.name), 2)
            label.verify(node)
        node.generate(101)

        self.log.info('- Check that setlabel can assign a label to a new unused address.')
        for label in labels:
            address = node.getnewaddress()
            node.setlabel(address, label.name)
            label.add_address(address)
            label.verify(node)
            addresses = node.getaddressesbylabel("")
            assert not address in addresses.keys()

        # Check that addmultisigaddress can assign labels.
        for label in labels:
            addresses = []
            for x in range(10):
                addresses.append(node.getnewaddress())
            multisig_address = node.addmultisigaddress(5, addresses, label.name)['address']
            label.add_address(multisig_address)
            label.purpose[multisig_address] = "send"
            label.verify(node)
        node.generate(101)

        # Check that setlabel can change the label of an address from a
        # different label.
        change_label(node, labels[0].addresses[0], labels[0], labels[1])

        # Check that setlabel can set the label of an address already
        # in the label. This is a no-op.
        change_label(node, labels[2].addresses[0], labels[2], labels[2])

class Label:
    def __init__(self, name):
        # Label name
        self.name = name
        # Current receiving address associated with this label.
        self.receive_address = None
        # List of all addresses assigned with this label
        self.addresses = []
        # Map of address to address purpose
        self.purpose = defaultdict(lambda: "receive")

    def add_address(self, address):
        assert_equal(address not in self.addresses, True)
        self.addresses.append(address)

    def add_receive_address(self, address):
        self.add_address(address)

    def verify(self, node):
        if self.receive_address is not None:
            assert self.receive_address in self.addresses

        for address in self.addresses:
            assert_equal(
                node.getaddressinfo(address)['labels'][0],
                {"name": self.name,
                 "purpose": self.purpose[address],
                 "timestamp": 1500000000})
            assert_equal(node.getaddressinfo(address)['label'], self.name)

        assert_equal(
            node.getaddressesbylabel(self.name),
            {address: {"purpose": self.purpose[address], "timestamp": 1500000000} for address in self.addresses})

def change_label(node, address, old_label, new_label):
    assert_equal(address in old_label.addresses, True)
    node.setlabel(address, new_label.name)

    old_label.addresses.remove(address)
    new_label.add_address(address)

    old_label.verify(node)
    new_label.verify(node)

if __name__ == '__main__':
    WalletLabelsTest().main()
