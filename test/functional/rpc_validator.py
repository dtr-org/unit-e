#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_raises_rpc_error


class RPCFinalizationValidatorInfoTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 1

        self.setup_clean_chain = True

    def run_test(self):
        assert_raises_rpc_error(-32600, "The node must be a validator", self.nodes[0].getvalidatorinfo)


if __name__ == '__main__':
    RPCFinalizationValidatorInfoTest().main()
