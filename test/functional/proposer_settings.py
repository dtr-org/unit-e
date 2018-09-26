#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class ProposerSettingsTest(UnitETestFramework):

    """ This test checks that the node is not proposing actually when started
        with the -proposing flag turned off. """

    """ This test checks that the node is not proposing actually when started
        with the -proposing flag turned off. """

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-proposing=0']]

    def run_test(self):

        status = self.nodes[0].proposerstatus()['wallets']
        assert_equal(status[0]['status'], 'NOT_PROPOSING')
        assert_equal(status[0]['wallet'], 'wallet.dat')

        print("Test succeeded.")

if __name__ == '__main__':
    ProposerSettingsTest().main()
