#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.mininode import sha256
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.script import CScript, OP_2, hash160
from test_framework.test_framework import UnitETestFramework, STAKE_SPLIT_THRESHOLD
from test_framework.util import (
    assert_equal,
    bytes_to_hex_str,
    hex_str_to_bytes,
    wait_until,
    connect_nodes_bi,
)


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

    addr_info = staking_node.getaddressinfo(staking_node.getnewaddress('', 'legacy'))
    staking_key_hash = hash160(hex_str_to_bytes(addr_info['pubkey']))

    rs_p2wsh = CScript([OP_2, staking_key_hash, spending_script_hash])
    outputs = [
        {'address': 'script', 'amount': amount, 'script': bytes_to_hex_str(rs_p2wsh)}
    ]
    return node.sendtypeto('unite', 'unite', outputs)


class ProposerRemoteStakingTest(UnitETestFramework):

    """ This test checks that the proposer can use remote staking outputs for proposing. """

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            ['-proposing=0'],
            ['-proposing=1', '-stakesplitthreshold=1000000000000'],  # 10 000 UTE
        ]

    def setup_network(self):
        # Do not connect nodes
        self.setup_nodes()

    def run_test(self):
        alice, bob = self.nodes
        alice.importmasterkey(regtest_mnemonics[0]['mnemonics'])

        alice.generate(1)
        assert_equal(
            len(alice.listunspent()),
            regtest_mnemonics[0]['balance'] / STAKE_SPLIT_THRESHOLD,
        )

        ps = bob.proposerstatus()
        assert_equal(ps['wallets'][0]['stakeable_balance'], 0)

        alices_addr = alice.getnewaddress()

        bobs_addr = bob.getnewaddress('', 'bech32')

        self.log.info('Delegate staking of 6000 to Bob')
        tx1_id = alice.stakeat({'address': bobs_addr, 'amount': 3000})
        tx2_id = stake_p2wsh(alice, staking_node=bob, amount=3000)
        block_hash = alice.generatetoaddress(1, alices_addr)[0]

        rsp2wpkh_out = next(
            out
            for out in alice.getrawtransaction(tx1_id, True, block_hash)['vout']
            if out['value'] == 3000
        )
        rsp2wsh_out = next(
            out
            for out in alice.getrawtransaction(tx2_id, True, block_hash)['vout']
            if out['value'] == 3000
        )
        scripts = {
            (tx1_id, rsp2wpkh_out['n']): rsp2wpkh_out['scriptPubKey']['hex'],
            (tx2_id, rsp2wsh_out['n']): rsp2wsh_out['scriptPubKey']['hex'],
        }

        wi = alice.getwalletinfo()
        assert_equal(wi['remote_staking_balance'], 6000)

        initial_height = alice.getblockcount()
        connect_nodes_bi(self.nodes, 0, 1)
        self.sync_all()

        bob.proposerwake()
        self.log.info('Waiting for Bob to start proposing')
        wait_until(
            lambda: bob.proposerstatus()['wallets'][0]['status'] == 'IS_PROPOSING'
        )

        def bob_is_staking_the_new_coin():
            ps = bob.proposerstatus()
            return ps['wallets'][0]['stakeable_balance'] == 6000

        wait_until(bob_is_staking_the_new_coin, timeout=10)

        self.log.info('Waiting for Bob to propose a block')
        wait_until(lambda: bob.getblockcount() > initial_height)

        block_hash = bob.getbestblockhash()
        coinbase_id = bob.getblock(block_hash, 1)['tx'][0]
        coinbase = bob.getrawtransaction(coinbase_id, True, block_hash)
        assert_equal(len(coinbase['vin']), 2)
        assert_equal(len(coinbase['vout']), 2)
        assert_equal(coinbase['vout'][1]['value'], 3000)

        stake = (coinbase['vin'][1]['txid'], coinbase['vin'][1]['vout'])
        assert_equal(coinbase['vout'][1]['scriptPubKey']['hex'], scripts[stake])


if __name__ == '__main__':
    ProposerRemoteStakingTest().main()
