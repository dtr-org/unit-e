#!/usr/bin/env python3
# Copyright (c) 2014-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet accounts properly when there is a double-spend conflict."""
from decimal import Decimal

from test_framework.test_framework import (
    UnitETestFramework,
    PROPOSER_REWARD,
    FULL_FINALIZATION_REWARD,
)
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes,
    find_output,
    sync_blocks,
)


def create_and_sign_tx(node, inputs, outputs):
    """Create and sign transaction using node's RPC

    Args:
        node (TestNode): the node to sign transaction with
        inputs (list(dict)): utxos to use as inputs in the form [{"txid": txid, "vout": vout}]
        outputs (dict): outputs, where keys are the addresses and values the amounts
    """
    rawtx = node.createrawtransaction(inputs, outputs)
    signresult = node.signrawtransactionwithwallet(rawtx)
    assert_equal(signresult["complete"], True)
    return signresult["hex"]


class TxnMallTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.extra_args = [['-maxtipage=1000000000']] * self.num_nodes

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def add_options(self, parser):
        parser.add_argument("--mineblock", dest="mine_block", default=False, action="store_true",
                            help="Test double-spend of 1-confirmed transaction")

    def setup_network(self):
        # Start with split network:
        super().setup_network()
        disconnect_nodes(self.nodes[1], 2)
        disconnect_nodes(self.nodes[2], 1)

    def run_test(self):
        starting_balance = self.nodes[0].getbalance()
        change_address = self.nodes[0].getnewaddress()

        foo_fund = 1219
        bar_fund = 29
        doublespend_amount = 1240
        tx_fee = Decimal('-.02')

        # Assign coins to foo and bar addresses:
        node0_address_foo = self.nodes[0].getnewaddress()
        fund_foo_txid = self.nodes[0].sendtoaddress(node0_address_foo, foo_fund)
        fund_foo_tx = self.nodes[0].gettransaction(fund_foo_txid)

        node0_address_bar = self.nodes[0].getnewaddress()
        fund_bar_txid = self.nodes[0].sendtoaddress(node0_address_bar, bar_fund)
        fund_bar_tx = self.nodes[0].gettransaction(fund_bar_txid)

        assert_equal(self.nodes[0].getbalance(),
                     starting_balance + fund_foo_tx["fee"] + fund_bar_tx["fee"])

        # Coins are sent to node1_address
        node1_address = self.nodes[1].getnewaddress()

        # Make sure doublespend uses the same coins as tx1 and tx2
        foo_coin = {"txid": fund_foo_txid, "vout": find_output(self.nodes[0], fund_foo_txid, foo_fund)}
        bar_coin = {"txid": fund_bar_txid, "vout": find_output(self.nodes[0], fund_bar_txid, bar_fund)}

        # Use raw transaction API to send doublespend_amount UTE to node1_address, but don't broadcast:
        inputs = [foo_coin, bar_coin]
        change_address = self.nodes[0].getnewaddress()
        outputs = {node1_address: doublespend_amount, change_address: foo_fund + bar_fund - doublespend_amount + tx_fee}
        doublespend_hex = create_and_sign_tx(self.nodes[0], inputs, outputs)

        # Spend the same two coins and send those transactions to node0
        tx1_hex = create_and_sign_tx(self.nodes[0], [foo_coin], {node1_address: foo_fund + tx_fee})
        txid1 = self.nodes[0].sendrawtransaction(tx1_hex)

        tx2_hex = create_and_sign_tx(self.nodes[0], [bar_coin], {node1_address: bar_fund + tx_fee})
        txid2 = self.nodes[0].sendrawtransaction(tx2_hex)

        # Have node0 mine a block:
        if (self.options.mine_block):
            self.nodes[0].generate(1)
            sync_blocks(self.nodes[0:2])

        tx1 = self.nodes[0].gettransaction(txid1)
        tx2 = self.nodes[0].gettransaction(txid2)

        # Node0's balance should be starting balance, plus 50UTE for another
        # matured block, minus what was sent to node1, and minus transaction fees:
        expected = starting_balance + fund_foo_tx["fee"] + fund_bar_tx["fee"]
        if self.options.mine_block:
            expected += PROPOSER_REWARD
        expected += tx1["amount"] + tx1["fee"]
        expected += tx2["amount"] + tx2["fee"]
        assert_equal(self.nodes[0].getbalance(), expected)

        if self.options.mine_block:
            assert_equal(tx1["confirmations"], 1)
            assert_equal(tx2["confirmations"], 1)
            # Node1's balance should be both transaction amounts:
            assert_equal(self.nodes[1].getbalance(), starting_balance - tx1["amount"] - tx2["amount"])
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

        # Both transactions should be conflicted, as they spend the same coins as doublespend, that node2 mined
        assert_equal(tx1["confirmations"], -2)
        assert_equal(tx2["confirmations"], -2)

        self.nodes[0].generate(1)

        # Node0's total balance should be starting balance, plus 150UTE for
        # three more matured blocks (COINBASE_MATURITY deep), minus 1240 for the double-spend,
        # plus fees (which are negative):
        expected = starting_balance + 3 * PROPOSER_REWARD - doublespend_amount + fund_foo_tx["fee"] + fund_bar_tx["fee"] + tx_fee
        assert_equal(self.nodes[0].getbalance(), expected)

        # Node1's balance should be its initial balance plus the doublespend:
        assert_equal(self.nodes[1].getbalance(), 10000 + 25 * PROPOSER_REWARD + 25 * FULL_FINALIZATION_REWARD + 1240)

if __name__ == '__main__':
    TxnMallTest().main()
