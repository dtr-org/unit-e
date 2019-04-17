#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
from test_framework.blocktools import (
    create_block,
    create_coinbase,
    get_tip_snapshot_meta,
    sign_coinbase,
)
from test_framework.mininode import (
    msg_witness_block,
    network_thread_start,
    P2PInterface,
    sha256,
)
from test_framework.regtest_mnemonics import regtest_mnemonics
from test_framework.script import (
    CScript,
    OP_2,
    hash160,
)
from test_framework.test_framework import (
    UnitETestFramework,
    PROPOSER_REWARD,
    STAKE_SPLIT_THRESHOLD,
)
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    bytes_to_hex_str,
    hex_str_to_bytes,
    wait_until,
)


def stake_p2wsh(node, staking_node, amount):
    """
    Send funds to witness v2 remote staking output.

    Args:
        node: the node which will be able to spend funds
        staking_node: the node which will be able to stake nodes
        amount: the amount to send
    """
    multisig = node.addmultisigaddress(
        2, [node.getnewaddress(), node.getnewaddress()])
    bare = CScript(hex_str_to_bytes(multisig['redeemScript']))
    spending_script_hash = sha256(bare)

    addr_info = staking_node.validateaddress(
        staking_node.getnewaddress('', 'legacy'))
    staking_key_hash = hash160(hex_str_to_bytes(addr_info['pubkey']))

    rs_p2wsh = CScript([OP_2, staking_key_hash, spending_script_hash])
    outputs = [{'address': 'script', 'amount': amount,
                'script': bytes_to_hex_str(rs_p2wsh)}]
    return node.sendtypeto('unite', 'unite', outputs)


def build_block_with_remote_stake(node):
    height = node.getblockcount()
    snapshot_meta = get_tip_snapshot_meta(node)
    stakes = node.liststakeablecoins()

    coin = stakes['stakeable_coins'][0]['coin']
    script_pubkey = hex_str_to_bytes(coin['script_pub_key']['hex'])
    stake = {
        'txid': coin['out_point']['txid'],
        'vout': coin['out_point']['n'],
        'amount': coin['amount'],
    }

    tip = int(node.getbestblockhash(), 16)
    block_time = node.getblock(
        node.getbestblockhash())['time'] + 1
    coinbase = sign_coinbase(
        node, create_coinbase(
            height, stake, snapshot_meta.hash, raw_script_pubkey=script_pubkey))

    return create_block(tip, coinbase, block_time)


class RemoteStakingTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [
            [],
            ['-minimumchainwork=0', '-maxtipage=1000000000']
        ]

    def run_test(self):
        alice, bob = self.nodes
        alice.importmasterkey(regtest_mnemonics[0]['mnemonics'])

        bob.add_p2p_connection(P2PInterface())
        network_thread_start()
        bob.p2p.wait_for_verack()

        alice.generate(1)
        assert_equal(len(alice.listunspent()),
                     regtest_mnemonics[0]['balance'] / STAKE_SPLIT_THRESHOLD)

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
        tx1_hash = alice.stakeat(recipient)
        tx2_hash = stake_p2wsh(alice, staking_node=bob, amount=1)
        alice.generatetoaddress(1, alices_addr)
        self.sync_all()

        # Estimate Alice balance
        tx1_fee = alice.gettransaction(tx1_hash)['fee']
        tx2_fee = alice.gettransaction(tx2_hash)['fee']
        alice_balance = regtest_mnemonics[0]['balance'] + tx1_fee + tx2_fee

        wi = alice.getwalletinfo()
        assert_equal(wi['remote_staking_balance'], 2)
        assert_equal(wi['balance'], alice_balance)

        def bob_is_staking_the_new_coin():
            ps = bob.proposerstatus()
            return ps['wallets'][0]['stakeable_balance'] == 2
        wait_until(bob_is_staking_the_new_coin, timeout=10)

        # Bob generates a new block with remote stake of Alice
        block = build_block_with_remote_stake(bob)
        bob.p2p.send_and_ping(msg_witness_block(block))
        self.sync_all()

        # Reward from the Bob's block comes to remote staking balance of Alice
        # Actual spendable balance shouldn't change because the reward is not yet mature
        wi = alice.getwalletinfo()
        assert_equal(wi['remote_staking_balance'], 2 + PROPOSER_REWARD)
        assert_equal(wi['balance'], alice_balance)

        # Change outputs for both staked coins, and the balance staked remotely
        assert_equal(len(alice.listunspent()), 2 +
                     (regtest_mnemonics[0]['balance'] // STAKE_SPLIT_THRESHOLD))

        # Chech that Alice can spend remotely staked coins
        inputs = []
        for coin in bob.liststakeablecoins()['stakeable_coins']:
            out_point = coin['coin']['out_point']
            inputs.append({'tx': out_point['txid'], 'n': out_point['n']})
        alice.sendtypeto('', '', [{'address': alices_addr, 'amount': 1.9}], '', '', False,
                         {'changeaddress': alices_addr, 'inputs': inputs})

        wi = alice.getwalletinfo()
        assert_equal(wi['remote_staking_balance'], PROPOSER_REWARD)


if __name__ == '__main__':
    RemoteStakingTest().main()
