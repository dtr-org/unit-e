#!/usr/bin/env python3
# Copyright (c) 2017 The Particl Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import *
from test_framework.address import *

class FilterTransactionsTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.enable_mocktime()

    def run_test(self):
        # without argument
        txs = self.nodes[0].filtertransactions()
        assert len(txs) == 10

        # too many arguments
        assert_raises_rpc_error(-1, "filtertransactions", self.nodes[0].filtertransactions, "foo", "bar")

        self.test_output_format()
        self.test_count_option()
        self.test_skip_option()
        self.test_search()
        self.test_category_option()
        self.test_sort()

    def test_output_format(self):
        self.node_1_address = self.nodes[1].getnewaddress()
        simple_txid = self.nodes[0].sendtoaddress(self.node_1_address, 0.1)

        internal_transfer_txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 0.3)

        send_to = {self.nodes[0].getnewaddress(): 0.11,
                   self.nodes[1].getnewaddress(): 0.22,
                   self.nodes[0].getnewaddress(): 0.33,
                   self.node_1_address: 0.44}
        several_outs_txid = self.nodes[0].sendmany("", send_to)

        watchonly_address = self.nodes[0].getnewaddress()
        watchonly_pubkey = self.nodes[0].validateaddress(watchonly_address)["pubkey"]
        self.nodes[2].importpubkey(watchonly_pubkey, "", True)
        watchonly_txid = self.nodes[0].sendtoaddress(watchonly_address, Decimal("0.5"))

        self.sync_all()

        # simple send
        node_0_transactions = self.nodes[0].filtertransactions()
        assert_array_result(
            node_0_transactions,
            {"txid": simple_txid},
            {"category": "send", "amount": Decimal("-0.1"), "confirmations": 0}
        )
        result_tx = next(t for t in node_0_transactions if t["txid"] == simple_txid)
        assert_equal(len(result_tx["outputs"]), 1)
        assert_equal(result_tx["outputs"][0]["address"], self.node_1_address)
        assert_equal(result_tx["outputs"][0]["amount"], Decimal("-0.1"))

        node_1_transactions = self.nodes[1].filtertransactions()
        assert_array_result(
            node_1_transactions,
            {"txid": simple_txid},
            {"category": "receive", "amount": Decimal("0.1"), "confirmations": 0}
        )
        result_tx = next(t for t in node_1_transactions if t["txid"] == simple_txid)
        assert_equal(len(result_tx["outputs"]), 1)
        assert_equal(result_tx["outputs"][0]["address"], self.node_1_address)
        assert_equal(result_tx["outputs"][0]["amount"], Decimal("0.1"))

        # several outputs
        result_tx = next(t for t in self.nodes[0].filtertransactions() if t["txid"] == several_outs_txid)
        assert_equal(len(result_tx["outputs"]), 4)

        # internal transfer
        result_tx = next(t for t in self.nodes[0].filtertransactions() if t["txid"] == internal_transfer_txid)
        assert_equal(result_tx["category"], "internal_transfer")
        assert_equal(result_tx["amount"], Decimal("0"))
        assert_equal(result_tx["outputs"][0]["amount"], Decimal("0.3"))

        # watchonly
        node_2_transactions = self.nodes[2].filtertransactions({"include_watchonly": True})
        assert_array_result(
            node_2_transactions,
            {"txid": watchonly_txid},
            {"category": "receive", "amount": Decimal("0.5")}
        )
        result_tx = next(t for t in node_2_transactions if t["txid"] == watchonly_txid)
        assert_equal(result_tx["outputs"][0]["involvesWatchonly"], 1)

        result_tx = next(
            (t for t in self.nodes[2].filtertransactions({"include_watchonly": False}) if t["txid"] == watchonly_txid),
            None
        )
        assert result_tx is None

    def test_count_option(self):
        # count: -1 => JSONRPCException
        assert_raises_rpc_error(-8, "Invalid count", self.nodes[0].filtertransactions, {"count": -1})

        num_of_txs = len(self.nodes[0].filtertransactions({"count": 100}))

        # count: 0 => all transactions
        ro = self.nodes[0].filtertransactions({"count": 0})
        assert(len(ro) == num_of_txs)

        # count: 1
        ro = self.nodes[0].filtertransactions({"count": 1})
        assert(len(ro) == 1)

    def test_skip_option(self):
        num_of_txs = len(self.nodes[0].filtertransactions({"count": 100}))

        # skip: -1 => JSONRPCException
        assert_raises_rpc_error(-8, "Invalid skip", self.nodes[0].filtertransactions, {"skip": -1})

        # skip = count => no entry
        txs = self.nodes[0].filtertransactions({"skip": num_of_txs})
        assert(len(txs) == 0)

        # skip == count - 1 => one entry
        txs = self.nodes[0].filtertransactions({"skip": num_of_txs - 1})
        assert(len(txs) == 1)

    def test_search(self):
        queries = [
            (self.node_1_address, 2),
            ("440", 1)
        ]

        for query, expected_length in queries:
            txs = self.nodes[0].filtertransactions({"search": query})
            assert_equal(len(txs), expected_length)

    def test_category_option(self):
        categories = [
            ("internal_transfer", 2),
            ("coinbase", 10),
            ("send", 2),
            ("receive", 0)
        ]

        for category, expected_length in categories:
            txs = self.nodes[0].filtertransactions({"category": category})
            for tx in txs:
                assert_equal(tx["category"], category)
            assert_equal(len(txs), expected_length)

        # category 'all'
        num_of_txs = len(self.nodes[0].filtertransactions({"count": 20}))
        txs = self.nodes[0].filtertransactions({"category": "all", "count": 20})
        assert_equal(len(txs), num_of_txs)

        # invalid transaction category
        assert_raises_rpc_error(-8, "Invalid category", self.nodes[0].filtertransactions, {"category": "invalid"})

    def test_sort(self):
        sortings = [
            ("time", "desc"),
            ("address", "asc"),
            ("category", "asc"),
            ("amount", "desc"),
            ("confirmations", "desc"),
            ("txid", "asc")
        ]

        for sort_by, order in sortings:
            ro = self.nodes[0].filtertransactions({"sort": sort_by})
            prev = None
            for t in ro:
                if "address" not in t and "address" in t["outputs"][0]:
                    t["address"] = t["outputs"][0]["address"]
                if t["amount"] < 0:
                    t["amount"] = -t["amount"]
                if prev is not None:
                    if order == "asc":
                        assert_greater_than_or_equal(t[sort_by], prev[sort_by])
                    else:
                        assert_greater_than_or_equal(prev[sort_by], t[sort_by])
                prev = t

        # invalid sort
        assert_raises_rpc_error(-8, "Invalid sort", self.nodes[0].filtertransactions, {"sort": "invalid"})


if __name__ == "__main__":
    FilterTransactionsTest().main()
