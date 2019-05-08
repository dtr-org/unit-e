#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.util import assert_equal, wait_until
from test_framework.test_framework import UnitETestFramework


class ProposerMultiwalletTest(UnitETestFramework):

    """ This test checks that the proposer is picking up all the wallets. """

    def set_test_params(self):
        self.num_nodes = 1
        self.num_wallets = 3
        args = list('-wallet=w%s' % i for i in range(0, self.num_wallets))
        args.append("-proposing=1")
        self.extra_args = [args]

    def run_test(self):

        status = self.nodes[0].proposerstatus()['wallets']
        assert_equal(len(status), self.num_wallets)

        self.log.info("Checking that every wallet has a proposer")
        for i in range(0, self.num_wallets):
            assert_equal(status[i]['wallet'], 'w%d' % i)

        # the following checks that the proposer has advanced each
        # wallet from NOT_PROPOSING to some other state (which may
        # still be NOT_PROPOSING_SYNCHONG_BLOCKCHAIN or some other
        # message).
        def all_have_switched_away_from_not_proposing_state():
            status = self.nodes[0].proposerstatus()['wallets']
            return all(not status[i]['status'] == 'NOT_PROPOSING' for i in range(0, self.num_wallets))

        self.log.info("Waiting for all proposers to have advanced to some state")
        wait_until(all_have_switched_away_from_not_proposing_state, timeout=10)

        print("Test succeeded.")

if __name__ == '__main__':
    ProposerMultiwalletTest().main()
