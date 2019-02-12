#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.mininode import sha256
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.script import CScript, OP_2, hash160
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal, assert_greater_than, hex_str_to_bytes, wait_until


def stake_p2wsh(node, staking_node, amount):
    """
    Send funds to witness v2 remote staking output.

    Args:
        node: the node which will be able to spend funds
        staking_node: the node which will be able to stake nodes
        amount: the amount to send
    """
    multisig = node.addmultisigaddress(2, [node.getnewaddress(), node.getnewaddress()])
    bare = CScript(hex_str_to_bytes(multisig['redeemScript']))
    spending_script_hash = sha256(bare)

    addr_info = staking_node.validateaddress(staking_node.getnewaddress('', 'legacy'))
    staking_key_hash = hash160(hex_str_to_bytes(addr_info['pubkey']))

    rs_p2wsh = CScript([OP_2, staking_key_hash, spending_script_hash])
    outputs = [{'address': 'script', 'amount': amount, 'script': rs_p2wsh.hex()}]
    node.sendtypeto('unite', 'unite', outputs)


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
        stake_p2wsh(alice, staking_node=bob, amount=1)
        alice.generatetoaddress(1, alices_addr)
        self.sync_all()

        wi = alice.getwalletinfo()
        assert_equal(wi['remote_staking_balance'], 2)

        def bob_is_staking_the_new_coin():
            ps = bob.proposerstatus()
            return ps['wallets'][0]['stakeable_balance'] == 2
        wait_until(bob_is_staking_the_new_coin, timeout=10)

        # Change output, and the balance staked remotely
        assert_equal(len(alice.listunspent()), 3)


if __name__ == '__main__':
    RemoteStakingTest().main()
