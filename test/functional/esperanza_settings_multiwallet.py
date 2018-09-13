#!/usr/bin/env python3
# Copyright (c) 2014-2017 The unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class EsperanzaTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[
            '-wallet=w1.dat',
            '-wallet=w2.dat',
            '-wallet=w3.dat'
        ]]

    def run_test(self):

        status = self.nodes[0].proposerstatus()
        assert_equal(len(status), 3)
        assert_equal(status[0]['wallet'], 'w1.dat')
        assert_equal(status[1]['wallet'], 'w2.dat')
        assert_equal(status[2]['wallet'], 'w3.dat')

        print("Test succeeded.")

        return

if __name__ == '__main__':
    EsperanzaTest().main()
