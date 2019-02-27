#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet accounts properly when there is a double-spend conflict."""

from test_framework.test_framework import UnitETestFramework
from test_framework.util import *


# UNIT-E TODO: adjust the one in util.py
# inputs should be a list of tuples cointaining txid and vout: [(txid, vout)]
# outputs should be a list of tuples containing address and amount: [(address, amount)]
def create_tx(node, inputs, outputs):
    inputs = [{"txid": _input[0], "vout": _input[1]} for _input in inputs]
    outputs = {to_address: amount for to_address, amount in outputs}
    rawtx = node.createrawtransaction(inputs, outputs)
    signresult = node.signrawtransaction(rawtx)
    assert_equal(signresult["complete"], True)
    return signresult["hex"]


class TxnMallTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4

    def add_options(self, parser):
        parser.add_option("--mineblock", dest="mine_block", default=True, action="store_true",
                          help="Test double-spend of 1-confirmed transaction")

    def setup_network(self):
        # Start with split network:
        super().setup_network()
        disconnect_nodes(self.nodes[1], 2)
        disconnect_nodes(self.nodes[2], 1)

    def run_test(self):
        starting_balance = self.nodes[0].getbalance("")
        change_address = self.nodes[0].getnewaddress()

        foo_fund = 1219
        bar_fund = 29
        doublespend_amount = 1240
        tx_fee = Decimal('-.02')

        # Assign coins to foo and bar accounts:
        node0_address_foo = self.nodes[0].getnewaddress("foo")
        fund_foo_txid = self.nodes[0].sendfrom("", node0_address_foo, foo_fund)
        fund_foo_tx = self.nodes[0].gettransaction(fund_foo_txid)

        node0_address_bar = self.nodes[0].getnewaddress("bar")
        fund_bar_txid = self.nodes[0].sendfrom("", node0_address_bar, bar_fund)
        fund_bar_tx = self.nodes[0].gettransaction(fund_bar_txid)

        assert_equal(self.nodes[0].getbalance(""),
                     starting_balance - foo_fund - bar_fund + fund_foo_tx["fee"] + fund_bar_tx["fee"])

        # Coins are sent to node1_address
        node1_address = self.nodes[1].getnewaddress("from0")

        # Make sure doublespend uses the same coins as tx1 and tx2
        foo_coin = (fund_foo_txid, find_output(self.nodes[0], fund_foo_txid, foo_fund))
        bar_coin = (fund_bar_txid, find_output(self.nodes[0], fund_bar_txid, bar_fund))

        # First: use raw transaction API to send doublespend_amount UTE to node1_address,
        # but don't broadcast:
        inputs = [foo_coin, bar_coin]
        outputs = [(node1_address, doublespend_amount), (change_address, foo_fund + bar_fund - doublespend_amount + tx_fee)]
        doublespend_hex = create_tx(self.nodes[0], inputs, outputs)

        # Spend the same two coins and send those transactions to node0
        inputs = [foo_coin]
        outputs = [(node1_address, 40), (change_address, foo_fund - 40 + tx_fee)]
        tx1_hex = create_tx(self.nodes[0], inputs, outputs)
        txid1 = self.nodes[0].sendrawtransaction(tx1_hex)

        inputs = [bar_coin]
        outputs = [(node1_address, 20), (change_address, bar_fund - 20 + tx_fee)]
        tx2_hex = create_tx(self.nodes[0], inputs, outputs)
        txid2 = self.nodes[0].sendrawtransaction(tx2_hex)

        # Have node0 mine a block:
        if (self.options.mine_block):
            self.nodes[0].generate(1)
            sync_blocks(self.nodes[0:2])

        tx1 = self.nodes[0].gettransaction(txid1)
        tx2 = self.nodes[0].gettransaction(txid2)

        assert_equal(tx1['amount'], -40)
        assert_equal(tx2['amount'], -20)

        # Node0's balance should be starting balance, plus 50UTE for another
        # matured block, minus 40, minus 20, and minus transaction fees:
        expected = starting_balance + tx1["amount"] + tx2["amount"]
        if self.options.mine_block: expected += 25
        assert_equal(self.nodes[0].getbalance("*"), expected)
        assert_equal(self.nodes[0].getbalance(""), expected - foo_fund - bar_fund)

        # foo and bar accounts should be debited:
        # UNIT-E TODO: they really should be debited
        assert_equal(self.nodes[0].getbalance("foo", 0), foo_fund)
        assert_equal(self.nodes[0].getbalance("bar", 0), bar_fund)
        #assert_equal(self.nodes[0].getbalance("foo", 0), foo_fund+tx1["amount"]+tx1["fee"])
        #assert_equal(self.nodes[0].getbalance("bar", 0), bar_fund+tx2["amount"]+tx2["fee"])

        if self.options.mine_block:
            assert_equal(tx1["confirmations"], 1)
            assert_equal(tx2["confirmations"], 1)
            # Node1's "from0" balance should be both transaction amounts:
            assert_equal(self.nodes[1].getbalance("from0"), -(tx1["amount"]+tx2["amount"]))
        else:
            assert_equal(tx1["confirmations"], 0)
            assert_equal(tx2["confirmations"], 0)

        # Now give doublespend and its parents to miner:
        self.nodes[2].sendrawtransaction(fund_foo_tx["hex"])
        self.nodes[2].sendrawtransaction(fund_bar_tx["hex"])
        doublespend_txid = self.nodes[2].sendrawtransaction(doublespend_hex)
        # ... mine a block...
        self.nodes[2].generate(1)

        # Reconnect the split network, and sync chain:
        connect_nodes(self.nodes[1], 2)
        self.nodes[2].generate(1)  # Mine another block to make sure we sync
        sync_blocks(self.nodes)
        assert_equal(self.nodes[0].gettransaction(doublespend_txid)["confirmations"], 2)

        # Re-fetch transaction info:
        tx1 = self.nodes[0].gettransaction(txid1)
        tx2 = self.nodes[0].gettransaction(txid2)

        # Both transactions should be conflicted
        assert_equal(tx1["confirmations"], -2)
        assert_equal(tx2["confirmations"], -2)

        # Node0's total balance should be starting balance, minus doublespend_amount for the double-spend,
        # plus fees (which are negative):
        expected = starting_balance - doublespend_amount + fund_foo_tx["fee"] + fund_bar_tx["fee"] + tx_fee
        assert_equal(self.nodes[0].getbalance("*"), expected)
        assert_equal(self.nodes[0].getbalance(""), expected - foo_fund - bar_fund)
        assert_equal(self.nodes[0].getbalance("foo"), foo_fund)
        assert_equal(self.nodes[0].getbalance("bar"), bar_fund)
        assert_equal(self.nodes[1].getbalance("from0"), doublespend_amount)

if __name__ == '__main__':
    TxnMallTest().main()

