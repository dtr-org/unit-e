#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test slash condition detection works after node restart.

1. Test usual restart.
2. Test reindex.
"""

from test_framework.messages import (
    CTransaction,
    FromHex,
    ToHex,
    TxType,
)
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    connect_nodes,
    disconnect_nodes,
    generate_block,
    wait_until,
)


class SlashRestart(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}'],
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}', '-validating=1'],
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}', '-validating=1'],
        ]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()
        p, f1, f2 = self.nodes
        connect_nodes(p, f1.index)
        connect_nodes(p, f2.index)

    def restart_node(self, node, reindex=False):
        tip_before = node.getbestblockhash()
        args = node.extra_args
        for i, arg in enumerate(args):
            if arg.startswith('reindex='):
                del args[i]
        super(SlashRestart, self).restart_node(node.index, args + ['-reindex={0}'.format(1 if reindex else 0)])
        wait_until(lambda: node.getbestblockhash() == tip_before)

    def setup_deposit(self, proposer, finalizers):
        for f in finalizers:
            f.new_address = f.getnewaddress("", "legacy")
            assert_equal(f.getbalance(), 10000)

        for f in finalizers:
            deptx = f.deposit(f.new_address, 1500)
            self.wait_for_transaction(deptx, nodes=[proposer])

        generate_block(proposer, count=14)
        assert_equal(proposer.getblockcount(), 15)

    def make_double_vote_tx(self, vote_tx, input_tx, proposer, finalizer):
        # To detect double vote, it's enough having two votes which are:
        # 1. from same finalizer
        # 2. with same source epoch
        # 3. with same target epoch
        # 4. with different target hash
        # So, make target hash different.
        vote = proposer.extractvotefromsignature(bytes_to_hex_str(vote_tx.vin[0].scriptSig))
        target_hash = list(vote['target_hash'])
        target_hash[0] = '0' if target_hash[0] == '1' else '1'
        vote['target_hash'] = "".join(target_hash)
        prev_tx = proposer.decoderawtransaction(ToHex(input_tx))
        vtx = finalizer.createvotetransaction(vote, prev_tx['txid'])
        vtx = finalizer.signrawtransactionwithwallet(vtx)
        vtx = FromHex(CTransaction(), vtx['hex'])
        return vtx

    def run_test(self):
        p, f1, f2 = self.nodes

        self.setup_stake_coins(p, f1, f2)
        self.generate_sync(p)

        self.log.info("Setup deposit")
        self.setup_deposit(p, [f1, f2])
        disconnect_nodes(p, f1.index)
        disconnect_nodes(p, f2.index)

        self.log.info("Generate few epochs")
        votes = self.generate_epoch(proposer=p, finalizer=f1, count=2)
        assert len(votes) != 0

        self.log.info("Check slashing condition after node restart")
        self.restart_node(p)
        vtx = self.make_double_vote_tx(votes[0], votes[-1], p, f1)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', p.sendrawtransaction, ToHex(vtx))
        wait_until(lambda: len(p.getrawmempool()) > 0, timeout=20)
        slash = FromHex(CTransaction(), p.getrawtransaction(p.getrawmempool()[0]))
        assert_equal(slash.get_type(), TxType.SLASH)
        self.log.info("Slashed")

        self.log.info("Generate few epochs")
        votes = self.generate_epoch(proposer=p, finalizer=f2, count=2)
        assert len(votes) != 0

        self.log.info("Check slashing condition after node restart with reindex")
        self.restart_node(p, reindex=True)
        vtx = self.make_double_vote_tx(votes[0], votes[-1], p, f2)
        assert_raises_rpc_error(-26, 'bad-vote-invalid', p.sendrawtransaction, ToHex(vtx))
        wait_until(lambda: len(p.getrawmempool()) > 0, timeout=20)
        slash = FromHex(CTransaction(), p.getrawtransaction(p.getrawmempool()[0]))
        assert_equal(slash.get_type(), TxType.SLASH)
        self.log.info("Slashed")


if __name__ == '__main__':
    SlashRestart().main()
