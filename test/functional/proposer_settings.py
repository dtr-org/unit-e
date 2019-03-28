#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class ProposerSettingsTest(UnitETestFramework):

    """ This test checks the node proposing status depending on the '-proposing' flag."""

    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [['-wallet=xanadu', '-proposing=0'], [], ['-proposing=1']]
        self.setup_clean_chain = True

    def run_test(self):
        self.setup_stake_coins(*self.nodes)

        # If we pass -proposing=0 then the node should not propose
        status = self.nodes[0].proposerstatus()['wallets']
        assert_equal(status[0]['status'], 'NOT_PROPOSING')
        assert_equal(status[0]['wallet'], 'xanadu')

        # If we don't pass -proposing then the node should not propose because of the default in regtest
        assert_equal(self.nodes[1].proposerstatus()['wallets'][0]['status'], 'NOT_PROPOSING')

        # Leave IBD
        self.nodes[2].generatetoaddress(1, self.nodes[2].getnewaddress("", "legacy"))

        # If we pass -proposing=1 then the node should propose
        wait_until(lambda: self.nodes[2].proposerstatus()['wallets'][0]['status'] == "IS_PROPOSING", timeout=150)

        print("Test succeeded.")

if __name__ == '__main__':
    ProposerSettingsTest().main()
