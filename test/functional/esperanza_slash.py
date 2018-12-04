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
        self.proposer.p2p.wait_for_verack()

        proposer.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        validator.importmasterkey(
            'chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')

        assert (validator.getbalance() == 10000)

        self.validator_address = validator.getnewaddress("", "legacy")
        self.validator_privkey = validator.dumpprivkey(self.validator_address)

        # wait for coinbase maturity
        for n in range(0, 119):
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

        self.wait_for_transaction(self.deposit_id, 60)

        # cast double vote
        target_hash = self.validator.getblockhash(14 * 10 - 1)
        deposit_tx = self.validator.getrawtransaction(self.deposit_id, 1)
        self.send_vote(deposit_tx, 13, 14, target_hash)

        others_fork_hash = '1230456000000000000000000000000000000000000000000000000000000321'
        self.send_vote(deposit_tx, 13, 14, others_fork_hash)

        # wait for slash transaction in mempool
        time.sleep(15)

        # the mempool should have size 1 since the vote already sent should have been removed cause is a conflict
        assert_equal(len(self.proposer.getrawmempool()), 1)
        slash_tx = self.proposer.decoderawtransaction(self.proposer.getrawtransaction(self.proposer.getrawmempool()[0]))

        # mine one more block
        block_hash = self.generate_block(proposer)

        # check that the slash transaction has been mined
        assert(slash_tx['txid'] in self.proposer.getblock(block_hash[0])['tx'])

        return

    def send_vote(self, prev_tx, source, target, target_hash):

        vote_data = Vote(self.validator_address, target_hash, int(source), int(target))
        key = CECKey()
        key.set_secretbytes(base58_to_bytes(self.validator_privkey)[:-4])
        vote_sig = key.sign(vote_data.get_hash())

        tx = CTransaction()
        tx.nVersion = 0x00030001  # TxType::VOTE
        script_sig = CScript([vote_data.serialize(vote_sig)])
        tx.vin.append(CTxIn(COutPoint(int(prev_tx['txid'], 16), 0), script_sig))
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


if __name__ == '__main__':
    EsperanzaSlashTest().main()
    addr = 'muUx4dQ4bwssNQYpUqAJHSJCUonAZ4Ro2s'
    pkey = 'cNJWVLVrfrxZT85cwYfHdbRKGi2FQjkKFBjocwwinNNix5tytG33'
    source = 12
    target = 13
    target_hash = '4e7eae1625c033a05e92cff8d1591e4c7511888c264dbc8917ef94c3e66f22ef'

    decoded_addr = "9930bc5ed6f2342300091545eb54a4479bd500b2"
    print(decoded_addr + " : " + base58check_to_bytes(addr).hex())

    vote_hash = "7091ddf76382959df742734d6e29081dd251c674465794c30be6dc47670d4ae7"
    vote_data = Vote(addr, target_hash, int(source), int(target))
    print(vote_hash + " : " + hexlify(vote_data.get_hash()[::-1]).decode("ascii"))

    priv_key = "3081d302010104201581e5cab6229c8a0b5f1261769eec18ba671b465952eff1aa1ba4bd9bd6fec9a08185308182020101302c06072a8648ce3d0101022100fffffffffffffffffffffffffffffffffffffffffffffffffffffffefffffc2f300604010004010704210279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798022100fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141020101a12403220003e0525b038e4d0b2d019a9dff9443c4c4dbef9d731044ec2e1401867d0afef142"
    print(priv_key + " : " + hexlify(base58_to_bytes(pkey)[:-4]).decode("ascii"))

    key = CECKey()
    key.set_secretbytes(base58_to_bytes(pkey)[:-4])
    vote_sig = "3045022100c36ef3ed3b2c774d0acc9356c711e89cac1dae47503429486b40c5c9b3ff532c022045a34e6a99df92f76d4778f497deddaec388f92d3e65e2cdd1b76736af3a7c4d "
    print(vote_sig + " : " + hexlify(key.sign(vote_data.get_hash())).decode("ascii"))
