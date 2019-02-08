#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal, assert_greater_than


class RemoteStakingTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args=[
            ['-proposing=0'],
            ['-proposing=1', '-minimumchainwork=0', '-maxtipage=1000000000']
        ]

    def run_test(self):
        alice, bob = self.nodes
        alice.importmasterkey(regtest_mnemonics[0]['mnemonics'])

        alices_addr = alice.getnewaddress()

        # 'legacy': we need the PK hash, not a script hash
        bobs_addr = bob.getnewaddress('', 'legacy')

        # Estimate staking fee
        recipient = {"address": bobs_addr, "amount": 1}
        result = alice.stakeat(recipient, True)
        assert_greater_than(0.001, result['fee'])

        ps = bob.proposerstatus()
        assert_equal(ps['wallets'][0]['stakeable_balance'], 0)

        # Stake the funds
        result = alice.stakeat(recipient)
        alice.generatetoaddress(1, alices_addr)
        self.sync_all()

        bob.proposerwake()

        # Bob should be able to stake the newly received coin
        ps = bob.proposerstatus()
        assert_equal(ps['wallets'][0]['status'], 'IS_PROPOSING')
        assert_equal(ps['wallets'][0]['stakeable_balance'], 1)

        # Change output, and the balance staked remotely
        assert_equal(len(alice.listunspent()), 2)


if __name__ == '__main__':
    RemoteStakingTest().main()
