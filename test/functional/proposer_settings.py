#!/usr/bin/env python3
# Copyright (c) 2014-2017 The unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class EsperanzaTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-proposing=0']]

    def run_test(self):

        status = self.nodes[0].proposerstatus()['wallets']
        assert_equal(status[0]['status'], 'NOT_PROPOSING')
        assert_equal(status[0]['wallet'], 'wallet.dat')

        print("Test succeeded.")

        return

if __name__ == '__main__':
    EsperanzaTest().main()
