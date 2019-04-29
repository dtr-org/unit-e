#!/usr/bin/env python3
# Copyright (c) 2016-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test segwit transactions and blocks on P2P network."""

from test_framework.mininode import *
from test_framework.test_framework import UnitETestFramework, PROPOSER_REWARD
from test_framework.messages import msg_block, msg_block
from test_framework.util import *
from test_framework.script import *
from test_framework.blocktools import create_block, create_coinbase, sign_coinbase, get_tip_snapshot_meta
from test_framework.key import CECKey, CPubKey
import random
from binascii import hexlify

MAX_SIGOP_COST = 80000

# Calculate the virtual size of a witness block:
# (base + witness/4)
def get_virtual_size(witness_block):
    base_size = len(witness_block.serialize(with_witness=False))
    total_size = len(witness_block.serialize(with_witness=True))
    # the "+3" is so we round up
    vsize = int((3*base_size + total_size + 3)/4)
    return vsize

def test_transaction_acceptance(rpc, p2p, tx, with_witness, accepted, reason=None):
    """Send a transaction to the node and check that it's accepted to the mempool

    - Submit the transaction over the p2p interface
    - use the getrawmempool rpc to check for acceptance."""
    tx_message = msg_tx(tx)
    if with_witness:
        tx_message = msg_witness_tx(tx)
    p2p.send_message(tx_message)
    p2p.sync_with_ping()
    assert_equal(tx.hash in rpc.getrawmempool(), accepted)
    if (reason != None and not accepted):
        # Check the rejection reason as well.
        with mininode_lock:
            assert_equal(p2p.last_message["reject"].reason, reason)

def test_witness_block(rpc, p2p, block, accepted, with_witness=True):
    """Send a block to the node and check that it's accepted

    - Submit the block over the p2p interface
    - use the getbestblockhash rpc to check for acceptance."""
    p2p.send_message(msg_block(block))
    p2p.sync_with_ping()

    if accepted:
        assert_equal(rpc.getbestblockhash(), block.hash)
    else:
        assert_not_equal(rpc.getbestblockhash(), block.hash)

class TestNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.getdataset = set()

    def on_getdata(self, message):
        for inv in message.inv:
            self.getdataset.add(inv.hash)

    def announce_tx_and_wait_for_getdata(self, tx, timeout=60):
        with mininode_lock:
            self.last_message.pop("getdata", None)
        self.send_message(msg_inv(inv=[CInv(1, tx.sha256)]))
        self.wait_for_getdata(timeout)

    def announce_block_and_wait_for_getdata(self, block, use_header, timeout=60):
        with mininode_lock:
            self.last_message.pop("getdata", None)
            self.last_message.pop("getheaders", None)
        msg = msg_headers()
        msg.headers = [ CBlockHeader(block) ]
        if use_header:
            self.send_message(msg)
        else:
            self.send_message(msg_inv(inv=[CInv(2, block.sha256)]))
            self.wait_for_getheaders()
            self.send_message(msg)
        self.wait_for_block_request(block.sha256)

    def request_block(self, blockhash, inv_type, timeout=60):
        with mininode_lock:
            self.last_message.pop("block", None)
        self.send_message(msg_getdata(inv=[CInv(inv_type, blockhash)]))
        self.wait_for_block(blockhash, timeout)
        return self.last_message["block"].block

# Used to keep track of anyone-can-spend outputs that we can use in the tests
class UTXO():
    def __init__(self, sha256, n, nValue):
        self.sha256 = sha256
        self.n = n
        self.nValue = nValue

# Helper for getting the script associated with a P2PKH
def GetP2PKHScript(pubkeyhash):
    return CScript([CScriptOp(OP_DUP), CScriptOp(OP_HASH160), pubkeyhash, CScriptOp(OP_EQUALVERIFY), CScriptOp(OP_CHECKSIG)])

# Add signature for a P2PK witness program.
def sign_P2PK_witness_input(script, txTo, inIdx, hashtype, value, key):
    tx_hash = SegwitVersion1SignatureHash(script, txTo, inIdx, hashtype, value)
    signature = key.sign(tx_hash) + chr(hashtype).encode('latin-1')
    txTo.wit.vtxinwit[inIdx].scriptWitness.stack = [signature, script]
    txTo.rehash()


class SegWitTest(UnitETestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        # This test tests SegWit
        self.extra_args = [["-whitelist=127.0.0.1"], ["-whitelist=127.0.0.1", "-acceptnonstdtxn=0"]]

    def setup_network(self):
        self.setup_nodes()
        connect_nodes(self.nodes[0], 1)
        connect_nodes(self.nodes[0], 2)
        self.sync_all()

    ''' Helpers '''
    # Build a block on top of node0's tip.
    def build_next_block(self, nVersion=4):
        tip = self.nodes[0].getbestblockhash()
        height = self.nodes[0].getblockcount() + 1
        block_time = self.nodes[0].getblockheader(tip)["mediantime"] + 1
        meta = get_tip_snapshot_meta(self.nodes[0])
        coin = get_unspent_coins(self.nodes[0], 1)[0]
        coinbase = sign_coinbase(self.nodes[0], create_coinbase(height, coin, meta.hash))
        block = create_block(int(tip, 16), coinbase, block_time)
        block.nVersion = nVersion
        return block

    # Adds list of transactions to block, adds witness commitment, then solves.
    def update_witness_block_with_transactions(self, block, tx_list):
        assert all(tx.hash is not None for tx in tx_list)
        block.vtx.extend(tx_list)
        for tx in block.vtx:
            tx.rehash()
        block.ensure_ltor()
        block.compute_merkle_trees()
        block.solve()

    ''' Individual tests '''
    def test_witness_services(self):
        self.log.info("Verifying NODE_WITNESS service bit")
        assert (self.test_node.nServices & NODE_WITNESS) != 0


    # See if sending a regular transaction works, and create a utxo
    # to use in later tests.
    def test_non_witness_transaction(self):
        # Mine a block with an anyone-can-spend coinbase,
        # let it mature, then try to spend it.
        self.log.info("Testing non-witness transaction")
        block = self.build_next_block(nVersion=1)
        block.solve()
        self.test_node.send_message(msg_block(block))
        self.test_node.sync_with_ping() # make sure the block was processed
        txid = block.vtx[0].sha256

        self.nodes[0].generate(99) # let the block mature

        # Create a transaction that spends the coinbase
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(txid, 0), b""))
        tx.vout.append(CTxOut((PROPOSER_REWARD - 1) * UNIT, CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE])))
        tx.calc_sha256()

        # Check that serializing it with or without witness is the same
        # This is a sanity check of our testing framework.
        assert_equal(msg_tx(tx).serialize(), msg_witness_tx(tx).serialize())

        self.test_node.send_message(msg_witness_tx(tx))
        self.test_node.sync_with_ping() # make sure the tx was processed
        assert tx.hash in self.nodes[0].getrawmempool()
        # Save this transaction for later
        self.utxo.append(UTXO(tx.sha256, 0, (PROPOSER_REWARD - 1) * UNIT))
        self.nodes[0].generate(1)


    def test_witness_block_size(self):
        self.log.info("Testing witness block size limit")
        # TODO: Test that non-witness carrying blocks can't exceed 1MB
        # Skipping this test for now; this is covered in p2p-fullblocktest.py

        # Test that witness-bearing blocks are limited at ceil(base + wit/4) <= 1MB.
        block = self.build_next_block()

        assert len(self.utxo) > 0

        # Create a P2WSH transaction.
        # The witness program will be a bunch of OP_2DROP's, followed by OP_TRUE.
        # This should give us plenty of room to tweak the spending tx's
        # virtual size.
        NUM_DROPS = 200 # 201 max ops per script!
        NUM_OUTPUTS = 50

        witness_program = CScript([OP_2DROP]*NUM_DROPS + [OP_TRUE])
        witness_hash = uint256_from_str(sha256(witness_program))
        scriptPubKey = CScript([OP_0, ser_uint256(witness_hash)])

        prevout = COutPoint(self.utxo[0].sha256, self.utxo[0].n)
        value = self.utxo[0].nValue

        parent_tx = CTransaction()
        parent_tx.vin.append(CTxIn(prevout, b""))
        child_value = int(value/NUM_OUTPUTS)
        for i in range(NUM_OUTPUTS):
            parent_tx.vout.append(CTxOut(child_value, scriptPubKey))
        parent_tx.vout[0].nValue -= 50000
        assert parent_tx.vout[0].nValue > 0
        parent_tx.rehash()

        child_tx = CTransaction()
        for i in range(NUM_OUTPUTS):
            child_tx.vin.append(CTxIn(COutPoint(parent_tx.sha256, i), b""))
        child_tx.vout = [CTxOut(value - 100000, CScript([OP_TRUE]))]
        for i in range(NUM_OUTPUTS):
            child_tx.wit.vtxinwit.append(CTxInWitness())
            child_tx.wit.vtxinwit[-1].scriptWitness.stack = [b'a'*195]*(2*NUM_DROPS) + [witness_program]
        child_tx.rehash()
        self.update_witness_block_with_transactions(block, [parent_tx, child_tx])

        # Calculate exact additional bytes (get_virtual_size would round it)
        additional_bytes = 1 + (4 * MAX_BLOCK_BASE_SIZE) - (3 * len(block.serialize(with_witness=False)) + len(block.serialize(with_witness=True)))

        i = 0
        while additional_bytes > 0:
            # Add some more bytes to each input until we hit MAX_BLOCK_BASE_SIZE+1
            extra_bytes = min(additional_bytes, 55)
            child_tx.wit.vtxinwit[int(i/(2*NUM_DROPS))].scriptWitness.stack[i%(2*NUM_DROPS)] = b'a'*(195+extra_bytes)
            additional_bytes -= extra_bytes
            i += 1

        block.compute_merkle_trees()
        block.solve()
        segwit_size = 3 * len(block.serialize(with_witness=False)) + len(block.serialize(with_witness=True))
        assert_equal(segwit_size, (4 * MAX_BLOCK_BASE_SIZE) + 1)
        # Make sure that our test case would exceed the old max-network-message
        # limit
        assert len(block.serialize(True)) > 2*1024*1024

        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Now resize the second transaction to make the block fit.
        cur_length = len(child_tx.wit.vtxinwit[0].scriptWitness.stack[0])
        child_tx.wit.vtxinwit[0].scriptWitness.stack[0] = b'a'*(cur_length-1)
        block.compute_merkle_trees()
        block.solve()
        assert_equal(get_virtual_size(block), MAX_BLOCK_BASE_SIZE)

        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Update available utxo's
        self.utxo.pop(0)
        self.utxo.append(UTXO(child_tx.sha256, 0, child_tx.vout[0].nValue))


    # Consensus tests of extra witness data in a transaction.
    def test_extra_witness_data(self):
        self.log.info("Testing extra witness data in tx")

        assert len(self.utxo) > 0

        block = self.build_next_block()

        witness_program = CScript([OP_DROP, OP_TRUE])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        # First try extra witness data on a tx that doesn't require a witness
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        tx.vout.append(CTxOut(self.utxo[0].nValue-2000, scriptPubKey))
        tx.vout.append(CTxOut(1000, CScript([OP_TRUE]))) # non-witness output
        tx.wit.vtxinwit.append(CTxInWitness())
        tx.wit.vtxinwit[0].scriptWitness.stack = [CScript([])]
        tx.rehash()
        self.update_witness_block_with_transactions(block, [tx])

        # Extra witness data should not be allowed.
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Try extra signature data.  Ok if we're not spending a witness output.
        block.vtx[1].wit.vtxinwit = []
        block.vtx[1].vin[0].scriptSig = CScript([OP_0])
        block.vtx[1].rehash()
        block.compute_merkle_trees()
        block.solve()

        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Now try extra witness/signature data on an input that DOES require a
        # witness
        tx2 = CTransaction()
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, 0), b"")) # witness output
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, 1), b"")) # non-witness
        tx2.vout.append(CTxOut(tx.vout[0].nValue, CScript([OP_TRUE])))
        tx2.wit.vtxinwit.extend([CTxInWitness(), CTxInWitness()])
        tx2.wit.vtxinwit[0].scriptWitness.stack = [ CScript([CScriptNum(1)]), CScript([CScriptNum(1)]), witness_program ]
        tx2.wit.vtxinwit[1].scriptWitness.stack = [ CScript([OP_TRUE]) ]
        tx2.rehash()

        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx2])

        # This has extra witness data, so it should fail.
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Now get rid of the extra witness, but add extra scriptSig data
        tx2.vin[0].scriptSig = CScript([OP_TRUE])
        tx2.vin[1].scriptSig = CScript([OP_TRUE])
        tx2.wit.vtxinwit[0].scriptWitness.stack.pop(0)
        tx2.wit.vtxinwit[1].scriptWitness.stack = []
        tx2.rehash()
        block.compute_merkle_trees()
        block.solve()

        # This has extra signature data for a witness input, so it should fail.
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Now get rid of the extra scriptsig on the witness input, and verify
        # success (even with extra scriptsig data in the non-witness input)
        tx2.vin[0].scriptSig = b""
        tx2.rehash()
        block.compute_merkle_trees()
        block.solve()

        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Update utxo for later tests
        self.utxo.pop(0)
        self.utxo.append(UTXO(tx2.sha256, 0, tx2.vout[0].nValue))


    def test_max_witness_push_length(self):
        ''' Should only allow up to 520 byte pushes in witness stack '''
        self.log.info("Testing maximum witness push size")
        MAX_SCRIPT_ELEMENT_SIZE = 520
        assert len(self.utxo)

        block = self.build_next_block()

        witness_program = CScript([OP_DROP, OP_TRUE])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        tx.vout.append(CTxOut(self.utxo[0].nValue-1000, scriptPubKey))
        tx.rehash()

        tx2 = CTransaction()
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, 0), b""))
        tx2.vout.append(CTxOut(tx.vout[0].nValue-1000, CScript([OP_TRUE])))
        tx2.wit.vtxinwit.append(CTxInWitness())
        # First try a 521-byte stack element
        tx2.wit.vtxinwit[0].scriptWitness.stack = [ b'a'*(MAX_SCRIPT_ELEMENT_SIZE+1), witness_program ]
        tx2.rehash()

        self.update_witness_block_with_transactions(block, [tx, tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Now reduce the length of the stack element
        tx2.wit.vtxinwit[0].scriptWitness.stack[0] = b'a'*(MAX_SCRIPT_ELEMENT_SIZE)

        block.compute_merkle_trees()
        block.solve()
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Update the utxo for later tests
        self.utxo.pop()
        self.utxo.append(UTXO(tx2.sha256, 0, tx2.vout[0].nValue))

    def test_max_witness_program_length(self):
        # Can create witness outputs that are long, but can't be greater than
        # 10k bytes to successfully spend
        self.log.info("Testing maximum witness program length")
        assert len(self.utxo)
        MAX_PROGRAM_LENGTH = 10000

        # This program is 19 max pushes (9937 bytes), then 64 more opcode-bytes.
        long_witness_program = CScript([b'a'*520]*19 + [OP_DROP]*63 + [OP_TRUE])
        assert len(long_witness_program) == MAX_PROGRAM_LENGTH+1
        long_witness_hash = sha256(long_witness_program)
        long_scriptPubKey = CScript([OP_0, long_witness_hash])

        block = self.build_next_block()

        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        tx.vout.append(CTxOut(self.utxo[0].nValue-1000, long_scriptPubKey))
        tx.rehash()

        tx2 = CTransaction()
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, 0), b""))
        tx2.vout.append(CTxOut(tx.vout[0].nValue-1000, CScript([OP_TRUE])))
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[0].scriptWitness.stack = [b'a']*44 + [long_witness_program]
        tx2.rehash()

        self.update_witness_block_with_transactions(block, [tx, tx2])

        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Try again with one less byte in the witness program
        witness_program = CScript([b'a'*520]*19 + [OP_DROP]*62 + [OP_TRUE])
        assert len(witness_program) == MAX_PROGRAM_LENGTH
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        tx.vout[0] = CTxOut(tx.vout[0].nValue, scriptPubKey)
        tx.rehash()
        tx2.vin[0].prevout.hash = tx.sha256
        tx2.wit.vtxinwit[0].scriptWitness.stack = [b'a']*43 + [witness_program]
        tx2.rehash()
        block.vtx = [block.vtx[0]]
        self.update_witness_block_with_transactions(block, [tx, tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        self.utxo.pop()
        self.utxo.append(UTXO(tx2.sha256, 0, tx2.vout[0].nValue))


    def test_witness_input_length(self):
        ''' Ensure that vin length must match vtxinwit length '''
        self.log.info("Testing witness input length")
        assert len(self.utxo)

        witness_program = CScript([OP_DROP, OP_TRUE])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        # Create a transaction that splits our utxo into many outputs
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        nValue = self.utxo[0].nValue
        for i in range(10):
            tx.vout.append(CTxOut(int(nValue/10), scriptPubKey))
        tx.vout[0].nValue -= 1000
        assert tx.vout[0].nValue >= 0
        tx.rehash()

        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Try various ways to spend tx that should all break.
        # This "broken" transaction serializer will not normalize
        # the length of vtxinwit.
        class BrokenCTransaction(CTransaction):
            def serialize_with_witness(self):
                flags = 0
                if not self.wit.is_null():
                    flags |= 1
                r = b""
                r += struct.pack("<i", self.nVersion)
                if flags:
                    dummy = []
                    r += ser_vector(dummy)
                    r += struct.pack("<B", flags)
                r += ser_vector(self.vin)
                r += ser_vector(self.vout)
                if flags & 1:
                    r += self.wit.serialize()
                r += struct.pack("<I", self.nLockTime)
                return r

        tx2 = BrokenCTransaction()
        for i in range(10):
            tx2.vin.append(CTxIn(COutPoint(tx.sha256, i), b""))
        tx2.vout.append(CTxOut(nValue-3000, CScript([OP_TRUE])))

        # First try using a too long vtxinwit
        for i in range(11):
            tx2.wit.vtxinwit.append(CTxInWitness())
            tx2.wit.vtxinwit[i].scriptWitness.stack = [b'a', witness_program]
        tx2.rehash()

        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Now try using a too short vtxinwit
        tx2.wit.vtxinwit.pop()
        tx2.wit.vtxinwit.pop()

        block.vtx = [block.vtx[0]]
        self.update_witness_block_with_transactions(block, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Now make one of the intermediate witnesses be incorrect
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[-1].scriptWitness.stack = [b'a', witness_program]
        tx2.wit.vtxinwit[5].scriptWitness.stack = [ witness_program ]

        block.vtx = [block.vtx[0]]
        self.update_witness_block_with_transactions(block, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Fix the broken witness and the block should be accepted.
        tx2.wit.vtxinwit[5].scriptWitness.stack = [b'a', witness_program]
        block.vtx = [block.vtx[0]]
        self.update_witness_block_with_transactions(block, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        self.utxo.pop()
        self.utxo.append(UTXO(tx2.sha256, 0, tx2.vout[0].nValue))


    # Verify that mempool:
    # - rejects transactions with unnecessary/extra witnesses
    # - accepts transactions with valid witnesses
    # and that witness transactions are relayed to non-upgraded peers.
    def test_tx_relay(self):
        self.log.info("Testing relay of witness transactions")
        # Generate a transaction that doesn't require a witness, but send it
        # with a witness.  Should be rejected because we can't use a witness
        # when spending a non-witness output.
        assert len(self.utxo)
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        tx.vout.append(CTxOut(self.utxo[0].nValue - 1000, CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE])))
        tx.wit.vtxinwit.append(CTxInWitness())
        tx.wit.vtxinwit[0].scriptWitness.stack = [ b'a' ]
        tx.rehash()

        tx_hash = tx.sha256

        # Verify that unnecessary witnesses are rejected.
        self.test_node.announce_tx_and_wait_for_getdata(tx)
        assert_equal(len(self.nodes[0].getrawmempool()), 0)
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx, with_witness=True, accepted=False)

        # Verify that removing the witness succeeds.
        self.test_node.announce_tx_and_wait_for_getdata(tx)
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx, with_witness=False, accepted=True)

        # Now try to add extra witness data to a valid witness tx.
        witness_program = CScript([OP_TRUE])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])
        tx2 = CTransaction()
        tx2.vin.append(CTxIn(COutPoint(tx_hash, 0), b""))
        tx2.vout.append(CTxOut(tx.vout[0].nValue-1000, scriptPubKey))
        tx2.rehash()

        tx3 = CTransaction()
        tx3.vin.append(CTxIn(COutPoint(tx2.sha256, 0), b""))
        tx3.wit.vtxinwit.append(CTxInWitness())

        # Add too-large for IsStandard witness and check that it does not enter reject filter
        p2sh_program = CScript([OP_TRUE])
        p2sh_pubkey = hash160(p2sh_program)
        witness_program2 = CScript([b'a'*400000])
        tx3.vout.append(CTxOut(tx2.vout[0].nValue-1000, CScript([OP_HASH160, p2sh_pubkey, OP_EQUAL])))
        tx3.wit.vtxinwit[0].scriptWitness.stack = [witness_program2]
        tx3.rehash()

        # Node will not be blinded to the transaction
        self.std_node.announce_tx_and_wait_for_getdata(tx3)
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, tx3, True, False, b'tx-size')
        self.std_node.announce_tx_and_wait_for_getdata(tx3)
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, tx3, True, False, b'tx-size')

        # Remove witness stuffing, instead add extra witness push on stack
        tx3.vout[0] = CTxOut(tx2.vout[0].nValue - 1000, CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE]))
        tx3.wit.vtxinwit[0].scriptWitness.stack = [CScript([CScriptNum(1)]), witness_program ]
        tx3.rehash()

        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx2, with_witness=True, accepted=True)
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx3, with_witness=True, accepted=False)

        # Get rid of the extra witness, and verify acceptance.
        tx3.wit.vtxinwit[0].scriptWitness.stack = [ witness_program ]
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx3, with_witness=True, accepted=True)

        # Test that getrawtransaction returns correct witness information
        # hash, size, vsize
        raw_tx = self.nodes[0].getrawtransaction(tx3.hash, 1)
        assert_equal(int(raw_tx["hash"], 16), tx3.calc_sha256(True))
        assert_equal(raw_tx["size"], len(tx3.serialize_with_witness()))
        vsize = (len(tx3.serialize_with_witness()) + 3*len(tx3.serialize_without_witness()) + 3) / 4
        assert_equal(raw_tx["vsize"], vsize)
        assert_equal(len(raw_tx["vin"][0]["txinwitness"]), 1)
        assert_equal(raw_tx["vin"][0]["txinwitness"][0], hexlify(witness_program).decode('ascii'))
        assert vsize != raw_tx["size"]

        # Cleanup: mine the transactions and update utxo for next test
        self.nodes[0].generate(1)
        assert_equal(len(self.nodes[0].getrawmempool()),  0)

        self.utxo.pop(0)
        self.utxo.append(UTXO(tx3.sha256, 0, tx3.vout[0].nValue))


    # Test that block requests to NODE_WITNESS peer are with MSG_WITNESS_FLAG
    def test_block_relay(self):
        self.log.info("Testing block relay")

        blocktype = 2|MSG_WITNESS_FLAG

        # test_node has set NODE_WITNESS, so all getdata requests should be for
        # witness blocks.
        # Test announcing a block via inv results in a getdata, and that
        # announcing a version 4 or random VB block with a header results in a getdata
        block1 = self.build_next_block()
        block1.solve()

        self.test_node.announce_block_and_wait_for_getdata(block1, use_header=False)
        test_witness_block(self.nodes[0].rpc, self.test_node, block1, True)

        block2 = self.build_next_block(nVersion=4)
        block2.solve()

        self.test_node.announce_block_and_wait_for_getdata(block2, use_header=True)
        test_witness_block(self.nodes[0].rpc, self.test_node, block2, True)

        block3 = self.build_next_block(nVersion=4)
        block3.solve()
        self.test_node.announce_block_and_wait_for_getdata(block3, use_header=True)
        test_witness_block(self.nodes[0].rpc, self.test_node, block3, True)

        # Witness blocks and non-witness blocks should be different.
        # Verify rpc getblock() returns witness blocks, while
        # getdata respects the requested type.
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)
        # Now try to retrieve it...
        rpc_block = self.nodes[0].getblock(block.hash, False)
        non_wit_block = self.test_node.request_block(block.sha256, 2)
        wit_block = self.test_node.request_block(block.sha256, 2|MSG_WITNESS_FLAG)
        assert_equal(wit_block.serialize(True), hex_str_to_bytes(rpc_block))
        assert_equal(wit_block.serialize(False), non_wit_block.serialize())
        assert_equal(wit_block.serialize(True), block.serialize(True))

        # Test size, vsize, weight
        rpc_details = self.nodes[0].getblock(block.hash, True)
        assert_equal(rpc_details["size"], len(block.serialize(True)))
        assert_equal(rpc_details["strippedsize"], len(block.serialize(False)))
        weight = 3*len(block.serialize(False)) + len(block.serialize(True))
        assert_equal(rpc_details["weight"], weight)

    # V0 segwit outputs should be standard
    def test_standardness_v0(self):
        self.log.info("Testing standardness of v0 outputs")
        assert len(self.utxo)

        witness_program = CScript([OP_TRUE])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        p2sh_pubkey = hash160(witness_program)
        p2sh_scriptPubKey = CScript([OP_HASH160, p2sh_pubkey, OP_EQUAL])

        # First prepare a p2sh output (so that spending it will pass standardness)
        p2sh_tx = CTransaction()
        p2sh_tx.vin = [CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b"")]
        p2sh_tx.vout = [CTxOut(self.utxo[0].nValue-1000, p2sh_scriptPubKey)]
        p2sh_tx.rehash()

        # Mine it on test_node to create the confirmed output.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2sh_tx, with_witness=True, accepted=True)
        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # Now test standardness of v0 P2WSH outputs.
        # Start by creating a transaction with two outputs.
        tx = CTransaction()
        tx.vin = [CTxIn(COutPoint(p2sh_tx.sha256, 0), CScript([witness_program]))]
        tx.vout = [CTxOut(p2sh_tx.vout[0].nValue-10000, scriptPubKey)]
        tx.vout.append(CTxOut(8000, scriptPubKey)) # Might burn this later
        tx.rehash()

        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, tx, with_witness=True, accepted=True)

        # Now create something that looks like a P2PKH output. This won't be spendable.
        scriptPubKey = CScript([OP_0, hash160(witness_hash)])
        tx2 = CTransaction()
        # if tx was accepted, then we spend the second output.
        tx2.vin = [CTxIn(COutPoint(tx.sha256, 1), b"")]
        tx2.vout = [CTxOut(7000, scriptPubKey)]
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[0].scriptWitness.stack = [witness_program]
        tx2.rehash()

        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, tx2, with_witness=True, accepted=True)

        # Now update self.utxo for later tests.
        tx3 = CTransaction()
        # tx and tx2 were both accepted.  Don't bother trying to reclaim the
        # P2PKH output; just send tx's first output back to an anyone-can-spend.
        sync_mempools([self.nodes[0], self.nodes[1]])
        tx3.vin = [CTxIn(COutPoint(tx.sha256, 0), b"")]
        tx3.vout = [CTxOut(tx.vout[0].nValue - 1000, CScript([OP_TRUE, OP_DROP] * 15 + [OP_TRUE]))]
        tx3.wit.vtxinwit.append(CTxInWitness())
        tx3.wit.vtxinwit[0].scriptWitness.stack = [witness_program]
        tx3.rehash()
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx3, with_witness=True, accepted=True)

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)
        self.utxo.pop(0)
        self.utxo.append(UTXO(tx3.sha256, 0, tx3.vout[0].nValue))
        assert_equal(len(self.nodes[1].getrawmempool()), 0)


    # Verify that future segwit upgraded transactions are non-standard,
    # but valid in blocks.
    def test_segwit_versions(self):
        self.log.info("Testing standardness/consensus for segwit versions (0-16)")
        assert len(self.utxo)
        NUM_TESTS = 17 # will test OP_0, OP1, ..., OP_16
        if (len(self.utxo) < NUM_TESTS):
            tx = CTransaction()
            tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
            split_value = (self.utxo[0].nValue - 4000) // NUM_TESTS
            for i in range(NUM_TESTS):
                tx.vout.append(CTxOut(split_value, CScript([OP_TRUE])))
            tx.rehash()
            block = self.build_next_block()
            self.update_witness_block_with_transactions(block, [tx])
            test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)
            self.utxo.pop(0)
            for i in range(NUM_TESTS):
                self.utxo.append(UTXO(tx.sha256, i, split_value))

        sync_blocks(self.nodes)
        temp_utxo = []
        tx = CTransaction()
        count = 0
        witness_program = CScript([OP_TRUE])
        witness_hash = sha256(witness_program)
        assert_equal(len(self.nodes[1].getrawmempool()), 0)
        # Do not check OP_1 and OP_2 (remote staking) here as it is a standard version
        # and version 0 is enough to test spending standard transaction in a non-standard one
        for version in list(range(OP_3, OP_16+1)) + [OP_0]:
            count += 1
            # First try to spend to a future version segwit scriptPubKey.
            scriptPubKey = CScript([CScriptOp(version), witness_hash])
            tx.vin = [CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b"")]
            tx.vout = [CTxOut(self.utxo[0].nValue-1000, scriptPubKey)]
            tx.rehash()
            test_transaction_acceptance(self.nodes[1].rpc, self.std_node, tx, with_witness=True, accepted=False)
            test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx, with_witness=True, accepted=True)
            self.utxo.pop(0)
            temp_utxo.append(UTXO(tx.sha256, 0, tx.vout[0].nValue))

        self.nodes[0].generate(1) # Mine all the transactions
        sync_blocks(self.nodes)
        assert len(self.nodes[0].getrawmempool()) == 0

        # Finally, verify that version 0 -> version 3 transactions
        # are non-standard
        scriptPubKey = CScript([CScriptOp(OP_3), witness_hash])
        tx2 = CTransaction()
        tx2.vin = [CTxIn(COutPoint(tx.sha256, 0), b"")]
        tx2.vout = [CTxOut(tx.vout[0].nValue-1000, scriptPubKey)]
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[0].scriptWitness.stack = [ witness_program ]
        tx2.rehash()
        # Gets accepted to test_node, because standardness of outputs isn't
        # checked with fRequireStandard
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx2, with_witness=True, accepted=True)
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, tx2, with_witness=True, accepted=False)
        temp_utxo.pop() # last entry in temp_utxo was the output we just spent
        temp_utxo.append(UTXO(tx2.sha256, 0, tx2.vout[0].nValue))

        # Spend everything in temp_utxo back to an OP_TRUE output.
        tx3 = CTransaction()
        total_value = 0
        for i in temp_utxo:
            tx3.vin.append(CTxIn(COutPoint(i.sha256, i.n), b""))
            tx3.wit.vtxinwit.append(CTxInWitness())
            total_value += i.nValue
        tx3.wit.vtxinwit[-1].scriptWitness.stack = [witness_program]
        tx3.vout.append(CTxOut(total_value - 1000, CScript([OP_TRUE])))
        tx3.rehash()
        # Spending a higher version witness output is not allowed by policy,
        # even with fRequireStandard=false.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx3, with_witness=True, accepted=False)
        self.test_node.sync_with_ping()
        with mininode_lock:
            assert_in(b"reserved for soft-fork upgrades", self.test_node.last_message["reject"].reason)

        # Building a block with the transaction must be valid, however.
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx2, tx3])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)
        sync_blocks(self.nodes)

        # Add utxo to our list
        self.utxo.append(UTXO(tx3.sha256, 0, tx3.vout[0].nValue))


    def test_premature_coinbase_witness_spend(self):
        self.log.info("Testing premature coinbase witness spend")
        block = self.build_next_block()
        # Change the output of the block to be a witness output.
        witness_program = CScript([OP_TRUE])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])
        block.vtx[0].vout[0].scriptPubKey = scriptPubKey
        block.vtx[0] = sign_coinbase(self.nodes[0], block.vtx[0])
        # This next line will rehash the coinbase and update the merkle
        # root, and solve.
        self.update_witness_block_with_transactions(block, [])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        spend_tx = CTransaction()
        spend_tx.vin = [CTxIn(COutPoint(block.vtx[0].sha256, 0), b"")]
        spend_tx.vout = [CTxOut(block.vtx[0].vout[0].nValue, witness_program)]
        spend_tx.wit.vtxinwit.append(CTxInWitness())
        spend_tx.wit.vtxinwit[0].scriptWitness.stack = [ witness_program ]
        spend_tx.rehash()

        # Now test a premature spend.
        self.nodes[0].generate(98)
        sync_blocks(self.nodes)
        block2 = self.build_next_block()
        self.update_witness_block_with_transactions(block2, [spend_tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block2, accepted=False)

        # Advancing one more block should allow the spend.
        self.nodes[0].generate(1)
        block2 = self.build_next_block()
        self.update_witness_block_with_transactions(block2, [spend_tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block2, accepted=True)
        sync_blocks(self.nodes)


    def test_signature_version_1(self):
        self.log.info("Testing segwit signature hash version 1")
        key = CECKey()
        key.set_secretbytes(b"9")
        pubkey = CPubKey(key.get_pubkey())

        witness_program = CScript([pubkey, CScriptOp(OP_CHECKSIG)])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        # First create a witness output for use in the tests.
        assert len(self.utxo)
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        tx.vout.append(CTxOut(self.utxo[0].nValue-1000, scriptPubKey))
        tx.rehash()

        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx, with_witness=True, accepted=True)
        # Mine this transaction in preparation for following tests.
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)
        sync_blocks(self.nodes)
        self.utxo.pop(0)

        # Test each hashtype
        prev_utxo = UTXO(tx.sha256, 0, tx.vout[0].nValue)
        for sigflag in [ 0, SIGHASH_ANYONECANPAY ]:
            for hashtype in [SIGHASH_ALL, SIGHASH_NONE, SIGHASH_SINGLE]:
                hashtype |= sigflag
                block = self.build_next_block()
                tx = CTransaction()
                tx.vin.append(CTxIn(COutPoint(prev_utxo.sha256, prev_utxo.n), b""))
                tx.vout.append(CTxOut(prev_utxo.nValue - 1000, scriptPubKey))
                tx.wit.vtxinwit.append(CTxInWitness())
                # Too-large input value
                sign_P2PK_witness_input(witness_program, tx, 0, hashtype, prev_utxo.nValue+1, key)
                self.update_witness_block_with_transactions(block, [tx])
                test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

                # Too-small input value
                sign_P2PK_witness_input(witness_program, tx, 0, hashtype, prev_utxo.nValue-1, key)
                block.vtx.pop() # remove last tx
                self.update_witness_block_with_transactions(block, [tx])
                test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

                # Now try correct value
                sign_P2PK_witness_input(witness_program, tx, 0, hashtype, prev_utxo.nValue, key)
                block.vtx.pop()
                self.update_witness_block_with_transactions(block, [tx])
                test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

                prev_utxo = UTXO(tx.sha256, 0, tx.vout[0].nValue)

        # Test combinations of signature hashes.
        # Split the utxo into a lot of outputs.
        # Randomly choose up to 10 to spend, sign with different hashtypes, and
        # output to a random number of outputs.  Repeat NUM_TESTS times.
        # Ensure that we've tested a situation where we use SIGHASH_SINGLE with
        # an input index > number of outputs.
        NUM_TESTS = 500
        temp_utxos = []
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(prev_utxo.sha256, prev_utxo.n), b""))
        split_value = prev_utxo.nValue // NUM_TESTS
        for i in range(NUM_TESTS):
            tx.vout.append(CTxOut(split_value, scriptPubKey))
        tx.wit.vtxinwit.append(CTxInWitness())
        sign_P2PK_witness_input(witness_program, tx, 0, SIGHASH_ALL, prev_utxo.nValue, key)
        for i in range(NUM_TESTS):
            temp_utxos.append(UTXO(tx.sha256, i, split_value))

        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        block = self.build_next_block()
        used_sighash_single_out_of_bounds = False
        for i in range(NUM_TESTS):
            # Ping regularly to keep the connection alive
            if (not i % 100):
                self.test_node.sync_with_ping()
            # Choose random number of inputs to use.
            num_inputs = random.randint(1, 10)
            # Create a slight bias for producing more utxos
            num_outputs = random.randint(1, 11)
            random.shuffle(temp_utxos)
            assert len(temp_utxos) > num_inputs
            tx = CTransaction()
            total_value = 0
            for i in range(num_inputs):
                tx.vin.append(CTxIn(COutPoint(temp_utxos[i].sha256, temp_utxos[i].n), b""))
                tx.wit.vtxinwit.append(CTxInWitness())
                total_value += temp_utxos[i].nValue
            split_value = total_value // num_outputs
            for i in range(num_outputs):
                tx.vout.append(CTxOut(split_value, scriptPubKey))
            for i in range(num_inputs):
                # Now try to sign each input, using a random hashtype.
                anyonecanpay = 0
                if random.randint(0, 1):
                    anyonecanpay = SIGHASH_ANYONECANPAY
                hashtype = random.randint(1, 3) | anyonecanpay
                sign_P2PK_witness_input(witness_program, tx, i, hashtype, temp_utxos[i].nValue, key)
                if (hashtype == SIGHASH_SINGLE and i >= num_outputs):
                    used_sighash_single_out_of_bounds = True
            tx.rehash()
            for i in range(num_outputs):
                temp_utxos.append(UTXO(tx.sha256, i, split_value))
            temp_utxos = temp_utxos[num_inputs:]

            block.vtx.append(tx)

            # Test the block periodically, if we're close to maxblocksize
            if (get_virtual_size(block) > MAX_BLOCK_BASE_SIZE - 1000):
                self.update_witness_block_with_transactions(block, [])
                test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)
                block = self.build_next_block()

        if (not used_sighash_single_out_of_bounds):
            self.log.info("WARNING: this test run didn't attempt SIGHASH_SINGLE with out-of-bounds index value")
        # Test the transactions we've added to the block
        if (len(block.vtx) > 1):
            self.update_witness_block_with_transactions(block, [])
            test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Now test witness version 0 P2PKH transactions
        pubkeyhash = hash160(pubkey)
        scriptPKH = CScript([OP_0, pubkeyhash])
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(temp_utxos[0].sha256, temp_utxos[0].n), b""))
        tx.vout.append(CTxOut(temp_utxos[0].nValue, scriptPKH))
        tx.wit.vtxinwit.append(CTxInWitness())
        sign_P2PK_witness_input(witness_program, tx, 0, SIGHASH_ALL, temp_utxos[0].nValue, key)
        tx2 = CTransaction()
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, 0), b""))
        tx2.vout.append(CTxOut(tx.vout[0].nValue, CScript([OP_TRUE])))

        script = GetP2PKHScript(pubkeyhash)
        sig_hash = SegwitVersion1SignatureHash(script, tx2, 0, SIGHASH_ALL, tx.vout[0].nValue)
        signature = key.sign(sig_hash) + b'\x01' # 0x1 is SIGHASH_ALL

        # Check that we can't have a scriptSig
        tx2.vin[0].scriptSig = CScript([signature, pubkey])
        tx2.rehash()
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx, tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=False)

        # Move the signature to the witness.
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[0].scriptWitness.stack = [signature, pubkey]
        tx2.vin[0].scriptSig = b""
        tx2.rehash()

        self.update_witness_block_with_transactions(block, [])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        temp_utxos.pop(0)

        # Update self.utxos for later tests by creating two outputs
        # that consolidate all the coins in temp_utxos.
        output_value = sum(i.nValue for i in temp_utxos) // 2

        tx = CTransaction()
        index = 0
        # Just spend to our usual anyone-can-spend output
        tx.vout = [CTxOut(output_value, CScript([OP_TRUE]))] * 2
        for i in temp_utxos:
            # Use SIGHASH_ALL|SIGHASH_ANYONECANPAY so we can build up
            # the signatures as we go.
            tx.vin.append(CTxIn(COutPoint(i.sha256, i.n), b""))
            tx.wit.vtxinwit.append(CTxInWitness())
            sign_P2PK_witness_input(witness_program, tx, index, SIGHASH_ALL|SIGHASH_ANYONECANPAY, i.nValue, key)
            index += 1
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        for i in range(len(tx.vout)):
            self.utxo.append(UTXO(tx.sha256, i, tx.vout[i].nValue))


    # Test P2SH wrapped witness programs.
    def test_p2sh_witness(self):
        self.log.info("Testing P2SH witness transactions")

        assert len(self.utxo)

        # Prepare the p2sh-wrapped witness output
        witness_program = CScript([OP_DROP, OP_TRUE])
        witness_hash = sha256(witness_program)
        p2wsh_pubkey = CScript([OP_0, witness_hash])
        p2sh_witness_hash = hash160(p2wsh_pubkey)
        scriptPubKey = CScript([OP_HASH160, p2sh_witness_hash, OP_EQUAL])
        scriptSig = CScript([p2wsh_pubkey]) # a push of the redeem script

        # Fund the P2SH output
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        tx.vout.append(CTxOut(self.utxo[0].nValue-1000, scriptPubKey))
        tx.rehash()

        # Verify mempool acceptance and block validity
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx, with_witness=False, accepted=True)
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True, with_witness=True)
        sync_blocks(self.nodes)

        # Now test attempts to spend the output.
        spend_tx = CTransaction()
        spend_tx.vin.append(CTxIn(COutPoint(tx.sha256, 0), scriptSig))
        spend_tx.vout.append(CTxOut(tx.vout[0].nValue-1000, CScript([OP_TRUE])))
        spend_tx.rehash()

        # This transaction should not be accepted into the mempool.
        # Mempool acceptance will use SCRIPT_VERIFY_WITNESS which
        # will require a witness to spend a witness program.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, spend_tx, with_witness=False, accepted=False)

        # Try to put the witness script in the scriptSig, should also fail.
        spend_tx.vin[0].scriptSig = CScript([p2wsh_pubkey, b'a'])
        spend_tx.rehash()
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, spend_tx, with_witness=False, accepted=False)

        # Now put the witness script in the witness
        spend_tx.vin[0].scriptSig = scriptSig
        spend_tx.rehash()
        spend_tx.wit.vtxinwit.append(CTxInWitness())
        spend_tx.wit.vtxinwit[0].scriptWitness.stack = [ b'a', witness_program ]

        # Verify mempool acceptance
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, spend_tx, with_witness=True, accepted=True)
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [spend_tx])

        # Sending this with witnesses should be valid.
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Update self.utxo
        self.utxo.pop(0)
        self.utxo.append(UTXO(spend_tx.sha256, 0, spend_tx.vout[0].nValue))


    def test_witness_sigops(self):
        '''Ensure sigop counting is correct inside witnesses.'''
        self.log.info("Testing sigops limit")

        assert len(self.utxo)

        # Keep this under MAX_OPS_PER_SCRIPT (201)
        witness_program = CScript([OP_TRUE, OP_IF, OP_TRUE, OP_ELSE] + [OP_CHECKMULTISIG]*5 + [OP_CHECKSIG]*193 + [OP_ENDIF])
        witness_hash = sha256(witness_program)
        scriptPubKey = CScript([OP_0, witness_hash])

        sigops_per_script = 20*5 + 193*1
        # We'll produce 2 extra outputs, one with a program that would take us
        # over max sig ops, and one with a program that would exactly reach max
        # sig ops
        outputs = (MAX_SIGOP_COST // sigops_per_script) + 2
        extra_sigops_available = MAX_SIGOP_COST % sigops_per_script

        # We chose the number of checkmultisigs/checksigs to make this work:
        assert extra_sigops_available < 100 # steer clear of MAX_OPS_PER_SCRIPT

        # This script, when spent with the first
        # N(=MAX_SIGOP_COST//sigops_per_script) outputs of our transaction,
        # would push us just over the block sigop limit.
        witness_program_toomany = CScript([OP_TRUE, OP_IF, OP_TRUE, OP_ELSE] + [OP_CHECKSIG]*(extra_sigops_available + 1) + [OP_ENDIF])
        witness_hash_toomany = sha256(witness_program_toomany)
        scriptPubKey_toomany = CScript([OP_0, witness_hash_toomany])

        # If we spend this script instead, we would exactly reach our sigop
        # limit (for witness sigops).
        witness_program_justright = CScript([OP_TRUE, OP_IF, OP_TRUE, OP_ELSE] + [OP_CHECKSIG]*(extra_sigops_available) + [OP_ENDIF])
        witness_hash_justright = sha256(witness_program_justright)
        scriptPubKey_justright = CScript([OP_0, witness_hash_justright])

        # First split our available utxo into a bunch of outputs
        split_value = self.utxo[0].nValue // outputs
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))
        for i in range(outputs):
            tx.vout.append(CTxOut(split_value, scriptPubKey))
        tx.vout[-2].scriptPubKey = scriptPubKey_toomany
        tx.vout[-1].scriptPubKey = scriptPubKey_justright
        tx.rehash()

        block_1 = self.build_next_block()
        self.update_witness_block_with_transactions(block_1, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block_1, accepted=True)

        tx2 = CTransaction()
        # If we try to spend the first n-1 outputs from tx, that should be
        # too many sigops.
        total_value = 0
        for i in range(outputs-1):
            tx2.vin.append(CTxIn(COutPoint(tx.sha256, i), b""))
            tx2.wit.vtxinwit.append(CTxInWitness())
            tx2.wit.vtxinwit[-1].scriptWitness.stack = [ witness_program ]
            total_value += tx.vout[i].nValue
        tx2.wit.vtxinwit[-1].scriptWitness.stack = [ witness_program_toomany ]
        tx2.vout.append(CTxOut(total_value, CScript([OP_TRUE])))
        tx2.rehash()

        block_2 = self.build_next_block()
        self.update_witness_block_with_transactions(block_2, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block_2, accepted=False)

        # Try dropping the last input in tx2, and add an output that has
        # too many sigops (contributing to legacy sigop count).
        checksig_count = (extra_sigops_available // 4) + 1
        scriptPubKey_checksigs = CScript([OP_CHECKSIG]*checksig_count)
        tx2.vout.append(CTxOut(0, scriptPubKey_checksigs))
        tx2.vin.pop()
        tx2.wit.vtxinwit.pop()
        tx2.vout[0].nValue -= tx.vout[-2].nValue
        tx2.rehash()
        block_3 = self.build_next_block()
        self.update_witness_block_with_transactions(block_3, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block_3, accepted=False)

        # If we drop the last checksig in this output, the tx should succeed.
        block_4 = self.build_next_block()
        tx2.vout[-1].scriptPubKey = CScript([OP_CHECKSIG]*(checksig_count-1))
        tx2.rehash()
        self.update_witness_block_with_transactions(block_4, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block_4, accepted=True)

        # Reset the tip back down for the next test
        sync_blocks(self.nodes)
        for x in self.nodes:
            x.invalidateblock(block_4.hash)

        # Try replacing the last input of tx2 to be spending the last
        # output of tx
        block_5 = self.build_next_block()
        tx2.vout.pop()
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, outputs-1), b""))
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[-1].scriptWitness.stack = [ witness_program_justright ]
        tx2.rehash()
        self.update_witness_block_with_transactions(block_5, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block_5, accepted=True)

        # TODO: test p2sh sigop counting

    # Uncompressed pubkeys are no longer supported in default relay policy,
    # but (for now) are still valid in blocks.
    def test_uncompressed_pubkey(self):
        self.log.info("Testing uncompressed pubkeys")
        # Segwit transactions using uncompressed pubkeys are not accepted
        # under default policy, but should still pass consensus.
        key = CECKey()
        key.set_secretbytes(b"9")
        key.set_compressed(False)
        pubkey = CPubKey(key.get_pubkey())
        assert_equal(len(pubkey), 65) # This should be an uncompressed pubkey

        assert len(self.utxo) > 0
        utxo = self.utxo.pop(0)

        # Test 1: P2WPKH
        # First create a P2WPKH output that uses an uncompressed pubkey
        pubkeyhash = hash160(pubkey)
        scriptPKH = CScript([OP_0, pubkeyhash])
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(utxo.sha256, utxo.n), b""))
        tx.vout.append(CTxOut(utxo.nValue-1000, scriptPKH))
        tx.rehash()

        # Confirm it in a block.
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Now try to spend it. Send it to a P2WSH output, which we'll
        # use in the next test.
        witness_program = CScript([pubkey, CScriptOp(OP_CHECKSIG)])
        witness_hash = sha256(witness_program)
        scriptWSH = CScript([OP_0, witness_hash])

        tx2 = CTransaction()
        tx2.vin.append(CTxIn(COutPoint(tx.sha256, 0), b""))
        tx2.vout.append(CTxOut(tx.vout[0].nValue-1000, scriptWSH))
        script = GetP2PKHScript(pubkeyhash)
        sig_hash = SegwitVersion1SignatureHash(script, tx2, 0, SIGHASH_ALL, tx.vout[0].nValue)
        signature = key.sign(sig_hash) + b'\x01' # 0x1 is SIGHASH_ALL
        tx2.wit.vtxinwit.append(CTxInWitness())
        tx2.wit.vtxinwit[0].scriptWitness.stack = [ signature, pubkey ]
        tx2.rehash()

        # Should fail policy test.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx2, True, False, b'non-mandatory-script-verify-flag (Using non-compressed keys in segwit)')
        # But passes consensus.
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx2])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Test 2: P2WSH
        # Try to spend the P2WSH output created in last test.
        # Send it to a P2SH(P2WSH) output, which we'll use in the next test.
        p2sh_witness_hash = hash160(scriptWSH)
        scriptP2SH = CScript([OP_HASH160, p2sh_witness_hash, OP_EQUAL])
        scriptSig = CScript([scriptWSH])

        tx3 = CTransaction()
        tx3.vin.append(CTxIn(COutPoint(tx2.sha256, 0), b""))
        tx3.vout.append(CTxOut(tx2.vout[0].nValue-1000, scriptP2SH))
        tx3.wit.vtxinwit.append(CTxInWitness())
        sign_P2PK_witness_input(witness_program, tx3, 0, SIGHASH_ALL, tx2.vout[0].nValue, key)

        # Should fail policy test.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx3, True, False, b'non-mandatory-script-verify-flag (Using non-compressed keys in segwit)')
        # But passes consensus.
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx3])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Test 3: P2SH(P2WSH)
        # Try to spend the P2SH output created in the last test.
        # Send it to a P2PKH output, which we'll use in the next test.
        scriptPubKey = GetP2PKHScript(pubkeyhash)
        tx4 = CTransaction()
        tx4.vin.append(CTxIn(COutPoint(tx3.sha256, 0), scriptSig))
        tx4.vout.append(CTxOut(tx3.vout[0].nValue-1000, scriptPubKey))
        tx4.wit.vtxinwit.append(CTxInWitness())
        sign_P2PK_witness_input(witness_program, tx4, 0, SIGHASH_ALL, tx3.vout[0].nValue, key)

        # Should fail policy test.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx4, True, False, b'non-mandatory-script-verify-flag (Using non-compressed keys in segwit)')
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx4])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)

        # Test 4: Uncompressed pubkeys should still be valid in non-segwit
        # transactions.
        tx5 = CTransaction()
        tx5.vin.append(CTxIn(COutPoint(tx4.sha256, 0), b""))
        tx5.vout.append(CTxOut(tx4.vout[0].nValue-1000, CScript([OP_TRUE])))
        (sig_hash, err) = SignatureHash(scriptPubKey, tx5, 0, SIGHASH_ALL)
        signature = key.sign(sig_hash) + b'\x01' # 0x1 is SIGHASH_ALL
        tx5.vin[0].scriptSig = CScript([signature, pubkey])
        tx5.rehash()
        # Should pass policy and consensus.
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx5, True, True)
        block = self.build_next_block()
        self.update_witness_block_with_transactions(block, [tx5])
        test_witness_block(self.nodes[0].rpc, self.test_node, block, accepted=True)
        self.utxo.append(UTXO(tx5.sha256, 0, tx5.vout[0].nValue))

    def test_non_standard_witness(self):
        self.log.info("Testing detection of non-standard P2WSH witness")
        pad = chr(1).encode('latin-1')

        # Create scripts for tests
        scripts = []
        scripts.append(CScript([OP_DROP] * 100))
        scripts.append(CScript([OP_DROP] * 99))
        scripts.append(CScript([pad * 59] * 59 + [OP_DROP] * 60))
        scripts.append(CScript([pad * 59] * 59 + [OP_DROP] * 61))

        p2wsh_scripts = []

        assert len(self.utxo)
        tx = CTransaction()
        tx.vin.append(CTxIn(COutPoint(self.utxo[0].sha256, self.utxo[0].n), b""))

        # For each script, generate a pair of P2WSH and P2SH-P2WSH output.
        outputvalue = (self.utxo[0].nValue - 1000) // (len(scripts) * 2)
        for i in scripts:
            p2wsh = CScript([OP_0, sha256(i)])
            p2sh = hash160(p2wsh)
            p2wsh_scripts.append(p2wsh)
            tx.vout.append(CTxOut(outputvalue, p2wsh))
            tx.vout.append(CTxOut(outputvalue, CScript([OP_HASH160, p2sh, OP_EQUAL])))
        tx.rehash()
        txid = tx.sha256
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, tx, with_witness=False, accepted=True)

        self.nodes[0].generate(1)
        sync_blocks(self.nodes)

        # Creating transactions for tests
        p2wsh_txs = []
        p2sh_txs = []
        for i in range(len(scripts)):
            p2wsh_tx = CTransaction()
            p2wsh_tx.vin.append(CTxIn(COutPoint(txid,i*2)))
            p2wsh_tx.vout.append(CTxOut(outputvalue - 5000, CScript([OP_0, hash160(hex_str_to_bytes(""))])))
            p2wsh_tx.wit.vtxinwit.append(CTxInWitness())
            p2wsh_tx.rehash()
            p2wsh_txs.append(p2wsh_tx)
            p2sh_tx = CTransaction()
            p2sh_tx.vin.append(CTxIn(COutPoint(txid,i*2+1), CScript([p2wsh_scripts[i]])))
            p2sh_tx.vout.append(CTxOut(outputvalue - 5000, CScript([OP_0, hash160(hex_str_to_bytes(""))])))
            p2sh_tx.wit.vtxinwit.append(CTxInWitness())
            p2sh_tx.rehash()
            p2sh_txs.append(p2sh_tx)

        # Testing native P2WSH
        # Witness stack size, excluding witnessScript, over 100 is non-standard
        p2wsh_txs[0].wit.vtxinwit[0].scriptWitness.stack = [pad] * 101 + [scripts[0]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2wsh_txs[0], True, False, b'bad-witness-nonstandard')
        # Non-standard nodes should accept
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2wsh_txs[0], True, True)

        # Stack element size over 80 bytes is non-standard
        p2wsh_txs[1].wit.vtxinwit[0].scriptWitness.stack = [pad * 81] * 100 + [scripts[1]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2wsh_txs[1], True, False, b'bad-witness-nonstandard')
        # Non-standard nodes should accept
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2wsh_txs[1], True, True)
        # Standard nodes should accept if element size is not over 80 bytes
        p2wsh_txs[1].wit.vtxinwit[0].scriptWitness.stack = [pad * 80] * 100 + [scripts[1]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2wsh_txs[1], True, True)

        # witnessScript size at 3600 bytes is standard
        p2wsh_txs[2].wit.vtxinwit[0].scriptWitness.stack = [pad, pad, scripts[2]]
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2wsh_txs[2], True, True)
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2wsh_txs[2], True, True)

        # witnessScript size at 3601 bytes is non-standard
        p2wsh_txs[3].wit.vtxinwit[0].scriptWitness.stack = [pad, pad, pad, scripts[3]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2wsh_txs[3], True, False, b'bad-witness-nonstandard')
        # Non-standard nodes should accept
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2wsh_txs[3], True, True)

        # Repeating the same tests with P2SH-P2WSH
        p2sh_txs[0].wit.vtxinwit[0].scriptWitness.stack = [pad] * 101 + [scripts[0]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2sh_txs[0], True, False, b'bad-witness-nonstandard')
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2sh_txs[0], True, True)
        p2sh_txs[1].wit.vtxinwit[0].scriptWitness.stack = [pad * 81] * 100 + [scripts[1]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2sh_txs[1], True, False, b'bad-witness-nonstandard')
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2sh_txs[1], True, True)
        p2sh_txs[1].wit.vtxinwit[0].scriptWitness.stack = [pad * 80] * 100 + [scripts[1]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2sh_txs[1], True, True)
        p2sh_txs[2].wit.vtxinwit[0].scriptWitness.stack = [pad, pad, scripts[2]]
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2sh_txs[2], True, True)
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2sh_txs[2], True, True)
        p2sh_txs[3].wit.vtxinwit[0].scriptWitness.stack = [pad, pad, pad, scripts[3]]
        test_transaction_acceptance(self.nodes[1].rpc, self.std_node, p2sh_txs[3], True, False, b'bad-witness-nonstandard')
        test_transaction_acceptance(self.nodes[0].rpc, self.test_node, p2sh_txs[3], True, True)

        self.nodes[0].generate(1)  # Mine and clean up the mempool of non-standard node
        # Valid but non-standard transactions in a block should be accepted by standard node
        sync_blocks(self.nodes)
        assert_equal(len(self.nodes[0].getrawmempool()), 0)
        assert_equal(len(self.nodes[1].getrawmempool()), 0)

        self.utxo.pop(0)


    def run_test(self):
        # Setup the p2p connections and start up the network thread.
        # self.test_node sets NODE_WITNESS|NODE_NETWORK
        self.test_node = self.nodes[0].add_p2p_connection(TestNode(), services=NODE_NETWORK|NODE_WITNESS)
        # self.std_node is for testing node1 (fRequireStandard=true)
        self.std_node = self.nodes[1].add_p2p_connection(TestNode(), services=NODE_NETWORK|NODE_WITNESS)

        network_thread_start()

        self.setup_stake_coins(*self.nodes)

        # Split genesis funds to be able to create outputs later on
        self.nodes[0].generate(1)

        # Keep a place to store utxo's that can be used in later tests
        self.utxo = []

        # Test logic begins here
        self.test_node.wait_for_verack()

        self.log.info("Starting tests before segwit lock in:")

        self.test_witness_services() # Verifies NODE_WITNESS
        self.test_non_witness_transaction() # non-witness tx's are accepted

        # Test P2SH witness handling
        self.test_p2sh_witness()
        self.test_witness_block_size()
        self.test_extra_witness_data()
        self.test_max_witness_push_length()
        self.test_max_witness_program_length()
        self.test_witness_input_length()
        self.test_block_relay()
        self.test_tx_relay()
        self.test_standardness_v0()
        self.test_segwit_versions()
        self.test_premature_coinbase_witness_spend()
        self.test_uncompressed_pubkey()
        self.test_signature_version_1()
        self.test_non_standard_witness()
        self.test_witness_sigops()


if __name__ == '__main__':
    SegWitTest().main()
