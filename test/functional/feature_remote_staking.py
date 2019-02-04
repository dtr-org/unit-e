#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.test_framework import UnitETestFramework


class RemoteStakingTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def run_test(self):
        alice, bob = self.nodes
        dummy_addr = alice.getnewaddress()
        bobs_addr = bob.getnewaddress()

        alice.generatetoaddress(101, dummy_addr)
        self.sync_all()

        # print([x for x in alice.listunspent() if x['spendable']])

        # Estimate staking fee
        recipient = {"address": bobs_addr, "amount": 1}
        result = alice.stakeat(recipient, True)
        assert(result['fee'] < 0.001)

        # Stake the funds
        recipient = {"address": bobs_addr, "amount": 1}
        result = alice.stakeat(recipient)
        alice.generate(1)
        self.sync_all()

        # Coinbase, change output, and the balance staked remotely
        assert(len(alice.listunspent()) == 3)


if __name__ == '__main__':
    RemoteStakingTest().main()
