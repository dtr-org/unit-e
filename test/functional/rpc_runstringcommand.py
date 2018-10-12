#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the RPC call related to the runstringcommand command.

Test corresponds to code in rpc/misc.cpp.
"""

import time

from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, Decimal


class RunstringcommandTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [['-wallet=w1', '-wallet=w2'], []]

    def run_test(self):
        self._test_uptime()

    def _test_uptime(self):
        assert_raises_rpc_error(-8, 'Invalid method',
                                self.nodes[0].runstringcommand, 'runstringcommand', 'generate', 'w1', '1')
        assert_raises_rpc_error(-8, 'Parameters must all be strings',
                                self.nodes[0].runstringcommand, 'generate', 'w1', 101)

        resp = self.nodes[0].runstringcommand('generate', 'w1', '101')
        assert_equal(101, len(resp))
        assert_equal(Decimal(50), self.nodes[0].runstringcommand('getbalance', 'w1'))
        assert_equal(Decimal(0), self.nodes[0].runstringcommand('getbalance', 'w2'))

        # Default wallet
        assert_equal(Decimal(0), self.nodes[1].runstringcommand('getbalance', ''))


if __name__ == '__main__':
    RunstringcommandTest().main()
