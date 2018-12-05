#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.blocktools import *
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import CTransaction, CBlock
from test_framework.admin import Admin
from test_framework.address import *
from test_framework.key import CECKey, CPubKey


class Vote:

    def __init__(self, validator_address, target_hash, source_epoch, target_epoch):
        self.validator_address = validator_address
        self.target_hash = target_hash
        self.source_epoch = source_epoch
        self.target_epoch = target_epoch

    def serialize(self, signature):
        return CScript([
            signature,
            base58check_to_bytes(self.validator_address),
            hex_str_to_bytes(self.target_hash)[::-1],
            struct.pack("<I", self.source_epoch),
            struct.pack("<I", self.target_epoch)])

    def get_hash(self):
        ss = bytes()
        ss += base58check_to_bytes(self.validator_address)
        ss += hex_str_to_bytes(self.target_hash)[::-1]
        ss += struct.pack("<I", self.source_epoch)
        ss += struct.pack("<I", self.target_epoch)
        return hash256(ss)


class EsperanzaSlashTest(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 2

        params_data = {
            'epochLength': 10,
            'minDepositSize': 1500,
        }
        json_params = json.dumps(params_data)

        validator_node_params = [
            '-validating=1',
            '-proposing=0',
            '-debug=all',
            '-whitelist=127.0.0.1',
            '-esperanzaconfig=' + json_params
        ]
        proposer_node_params = ['-proposing=0', '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig=' + json_params]

        self.extra_args = [proposer_node_params,
                           validator_node_params,
                           ]
        self.setup_clean_chain = True

    def run_test(self):
        block_time = 0.2

        nodes = self.nodes
        self.proposer = nodes[0]
        self.validator = nodes[1]
        proposer = self.proposer
        validator = self.validator

        self.proposer.add_p2p_connection(P2PInterface())

        network_thread_start()

        # wait_for_verack ensures that the P2P connection is fully up.
        proposer.p2p.wait_for_verack()

        proposer.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        validator.importmasterkey(
            'chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')

        assert (validator.getbalance() == 10000)

        self.validator_address = validator.getnewaddress("", "legacy")
        self.validator_privkey = validator.dumpprivkey(self.validator_address)

        # generate the first epoch
        for n in range(0, 9):
            self.generate_block(proposer)

        sync_blocks(self.nodes)

        # generates 1 more block
        Admin.authorize_and_disable(self, proposer)

        self.deposit_amount = 1500
        self.deposit_id = validator.deposit(self.validator_address, self.deposit_amount)

        # generate 2 more epochs
        for n in range(0, 20):
            self.generate_block(proposer)
            sync_blocks(self.nodes)
            time.sleep(block_time)

        self.wait_for_transaction(self.deposit_id, 30)

        # cast double vote
        target_hash = self.validator.getblockhash(2 * 10 - 1)
        deposit_tx = self.validator.getrawtransaction(self.deposit_id, 1)
        self.send_vote(deposit_tx, 1, 2, target_hash)

        others_fork_hash = '1230456000000000000000000000000000000000000000000000000000000321'
        self.send_vote(deposit_tx, 1, 2, others_fork_hash)

        # wait for slash transaction in mempool
        wait_until(self.wait_for_slash, timeout=15)

        # the mempool should have size 1 since the vote already sent should have been removed cause is a conflict
        assert_equal(len(proposer.getrawmempool()), 1)
        slash_tx_id = proposer.getrawmempool()[0]

        # mine one more block
        block_hash = self.generate_block(proposer)

        # check that the slash transaction has been mined
        assert (slash_tx_id in proposer.getblock(block_hash[0])['tx'])

    def send_vote(self, prev_tx, source, target, target_hash):

        vote_data = Vote(self.validator_address, target_hash, int(source), int(target))
        key = CECKey()
        key.set_secretbytes(base58_to_bytes(self.validator_privkey)[:-4])
        vote_sig = key.sign(vote_data.get_hash())

        tx = CTransaction()
        tx.nVersion = 0x00030001  # TxType::VOTE
        tx.vin.append(CTxIn(COutPoint(int(prev_tx['txid'], 16), 0), vote_data.serialize(vote_sig)))
        tx.vout.append(CTxOut(int(prev_tx['vout'][0]['value'] * UNIT),
                              hex_str_to_bytes(prev_tx['vout'][0]['scriptPubKey']['hex'])))

        signresult = self.validator.signrawtransaction(ToHex(tx))['hex']
        f = BytesIO(hex_str_to_bytes(signresult))
        tx.deserialize(f)

        self.proposer.p2p.send_and_ping(msg_tx(tx))

    def generate_block(self, node):
        i = 0
        # It is rare but possible that a block was valid at the moment of creation but
        # invalid at submission. This is to account for those cases.
        while i < 5:
            try:
                return node.generate(1)
            except JSONRPCException as exp:
                i += 1
                print("error generating block:", exp.error)
        raise AssertionError("Node" + str(node.index) + " cannot generate block")

    def wait_for_slash(self):
        for tx_id in self.proposer.getrawmempool():
            try:
                raw_tx = self.proposer.getrawtransaction(tx_id)
            except JSONRPCException:  # in case the transaction we are looking for is already gone from the mempool
                continue

            if raw_tx:
                tx = self.proposer.decoderawtransaction(raw_tx)
                if tx['txtype'] == 5:
                    return True


if __name__ == '__main__':
    EsperanzaSlashTest().main()
