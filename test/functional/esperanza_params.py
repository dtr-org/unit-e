#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet."""
from test_framework.test_particl import ParticlTestFramework
from test_framework.util import *
import json


class EsperanzaTest(ParticlTestFramework):

    params_data = {}

    def set_test_params(self):
        self.num_nodes = 1

        self.params_data['epochLength'] = 10
        self.params_data['minDepositSize'] = 1000
        self.params_data['dynastyLogoutDelay'] = 2
        self.params_data['withdrawalEpochDelay'] = 20
        self.params_data['slashFractionMultiplier'] = 4
        self.params_data['bountyFractionDenominator'] = 50
        self.params_data['baseInterestFactor'] = 5
        self.params_data['basePenaltyFactor'] = 4
        json_params = json.dumps(self.params_data)

        node_params = [
            '-validating=0',
            '-staking=0',
            '-debug=all',
            '-esperanzaconfig='+json_params
        ]

        self.extra_args = [node_params]
        self.setup_clean_chain = True

    def run_test(self):

        nodes = self.nodes

        main_node = nodes[0]

        response = main_node.getesperanzaconfig()

        assert_equal(self.params_data['epochLength'], response['epochLength'])
        assert_equal(self.params_data['minDepositSize'], response['minDepositSize'])
        assert_equal(self.params_data['dynastyLogoutDelay'], response['dynastyLogoutDelay'])
        assert_equal(self.params_data['withdrawalEpochDelay'], response['withdrawalEpochDelay'])
        assert_equal(self.params_data['slashFractionMultiplier'], response['slashFractionMultiplier'])
        assert_equal(self.params_data['bountyFractionDenominator'], response['bountyFractionDenominator'])
        assert_equal(str(self.params_data['baseInterestFactor']), response['baseInterestFactor'])
        assert_equal("0.000000"+str(self.params_data['basePenaltyFactor']), response['basePenaltyFactor'])

        return

if __name__ == '__main__':
    EsperanzaTest().main()
