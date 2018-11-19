#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class ProposerMultiwalletTest(UnitETestFramework):

    """ This test checks that the proposer is picking up all the wallets. """

    def set_test_params(self):
        self.num_nodes = 1
        self.num_wallets = 3
        self.extra_args = [list('-wallet=w{0}.dat'.format(i) for i in range(0, self.num_wallets))]

    def run_test(self):

        status = self.nodes[0].proposerstatus()['wallets']
        assert_equal(len(status), self.num_wallets)
        for i in range(0, self.num_wallets):
            assert_equal(status[i]['wallet'], 'w{0}.dat'.format(i))
            # the following checks that the proposer has advanced each
            # wallet from NOT_PROPOSING to some other state (which may
            # still be NOT_PROPOSING_SYNCHONG_BLOCKCHAIN or some other
            # message).
            assert(not status[i]['status'] == 'NOT_PROPOSING')

        print("Test succeeded.")

if __name__ == '__main__':
    ProposerMultiwalletTest().main()
