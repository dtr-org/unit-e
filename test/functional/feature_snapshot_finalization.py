#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test snapshot and commits integration.

After fast sync node should contain actual finalization state
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
    assert_finalizationstate,
    assert_raises_rpc_error,
    bytes_to_hex_str,
    connect_nodes,
    disconnect_nodes,
    sync_blocks,
    sync_chain,
    wait_until,
)

def setup_deposit(self, proposer, validators):
    for i, n in enumerate(validators):
        n.new_address = n.getnewaddress("", "legacy")

        assert_equal(n.getbalance(), 10000)

    for n in validators:
        deptx = n.deposit(n.new_address, 1500)
        self.wait_for_transaction(deptx, nodes=[proposer])

    proposer.generatetoaddress(21, proposer.getnewaddress('', 'bech32'))

    assert_equal(proposer.getblockcount(), 22)

class SnapshotFinalization(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}'],
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}', '-validating=1'],
            ['-esperanzaconfig={"epochLength": 5, "minDepositSize": 1500}', '-prune=1', '-isd=1'],
        ]
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()
        p, v, s = self.nodes
        connect_nodes(p, v.index)

    def run_test(self):
        p, v, s = self.nodes

        self.setup_stake_coins(p, v)
        self.generate_sync(p, nodes=[p, v])

        self.log.info("Setup deposit")
        setup_deposit(self, p, [v])
        disconnect_nodes(p, v.index)

        self.log.info("Generate few epochs")
        votes = self.generate_epoch(epoch_length=5, proposer=p, finalizer=v, count=2)
        assert(len(votes) != 0)

        assert_equal(p.getblockcount(), 32)
        assert_finalizationstate(p, {'currentEpoch': 7,
                                     'lastJustifiedEpoch': 6,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        self.log.info("Connect fast-sync node")
        connect_nodes(s, p.index)
        sync_blocks([p, s])

        assert_finalizationstate(s, {'currentEpoch': 7,
                                     'lastJustifiedEpoch': 6,
                                     'lastFinalizedEpoch': 5,
                                     'validators': 1})

        self.log.info("Generate next epoch")
        votes += self.generate_epoch(epoch_length=5, proposer=p, finalizer=v, count=1)

        assert_equal(p.getblockcount(), 37)
        assert_finalizationstate(p, {'currentEpoch': 8,
                                     'lastJustifiedEpoch': 7,
                                     'lastFinalizedEpoch': 6,
                                     'validators': 1})

        sync_blocks([p, s])
        assert_finalizationstate(s, {'currentEpoch': 8,
                                     'lastJustifiedEpoch': 7,
                                     'lastFinalizedEpoch': 6,
                                     'validators': 1})

        self.log.info("Check slashig condition");
        # Create new vote with input=votes[-1] which attepts to make a double vote
        vote = s.extractvotefromsignature(bytes_to_hex_str(votes[0].vin[0].scriptSig))
        vote['target_epoch'] = vote['target_epoch'] + 1;
        prev_tx = s.decoderawtransaction(ToHex(votes[-1]))
        vtx = v.createvotetransaction(vote, prev_tx['txid'])
        vtx = v.signrawtransaction(vtx)
        vtx = FromHex(CTransaction(), vtx['hex'])
        assert_raises_rpc_error(-26, 'bad-vote-invalid-state', s.sendrawtransaction, ToHex(vtx))
        wait_until(lambda: len(s.getrawmempool()) > 0, timeout=20)
        slash = FromHex(CTransaction(), s.getrawtransaction(s.getrawmempool()[0]))
        assert_equal(slash.get_type(), TxType.SLASH)

if __name__ == '__main__':
    SnapshotFinalization().main()
