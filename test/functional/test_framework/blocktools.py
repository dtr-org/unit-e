#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Utilities for manipulating blocks and transactions."""

from .address import (
    key_to_p2sh_p2wpkh,
    key_to_p2wpkh,
    script_to_p2sh_p2wsh,
    script_to_p2wsh,
)
from .mininode import *
from .script import (
    CScript,
    CScriptNum,
    OP_0,
    OP_1,
    OP_CHECKMULTISIG,
    OP_CHECKSIG,
    OP_RETURN,
    OP_TRUE,
    hash160,
)
from .util import assert_equal

# Create a block (with regtest difficulty)
def create_block(hashprev, coinbase, nTime=None):
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time()+600)
    else:
        block.nTime = nTime
    block.hashPrevBlock = hashprev
    block.nBits = 0x207fffff # Will break after a difficulty adjustment...
    block.vtx.append(coinbase)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

# From BIP141
WITNESS_COMMITMENT_HEADER = b"\xaa\x21\xa9\xed"


def get_witness_script(witness_root, witness_nonce):
    witness_commitment = uint256_from_str(hash256(ser_uint256(witness_root)+ser_uint256(witness_nonce)))
    output_data = WITNESS_COMMITMENT_HEADER + ser_uint256(witness_commitment)
    return CScript([OP_RETURN, output_data])


# Update coinbase input witness according to BIP141
def update_uncommited_block_structures(block, nonce=0):
    # witness_nonce should go to coinbase witness.
    block.vtx[0].wit.vtxinwit = [CTxInWitness()]
    block.vtx[0].wit.vtxinwit[0].scriptWitness.stack = [ser_uint256(nonce)]


def should_add_witness_commitment(block):
    return not all(tx.wit.is_null() for tx in block.vtx)


def sign_transaction(node, tx):
    signresult = node.signrawtransaction(bytes_to_hex_str(tx.serialize()))
    tx = CTransaction()
    f = BytesIO(hex_str_to_bytes(signresult['hex']))
    tx.deserialize(f)
    return tx

# According to BIP141, blocks with witness rules active must commit to the
# hash of all in-block transactions including witness.
def add_witness_commitment(block, nonce=0):
    # First calculate the merkle root of the block's
    # transactions, with witnesses.
    witness_nonce = nonce
    witness_root = block.calc_witness_merkle_root()

    update_uncommited_block_structures(block, witness_nonce)

    # witness commitment is the last OP_RETURN output in coinbase
    block.vtx[0].vout.append(CTxOut(0, get_witness_script(witness_root, witness_nonce)))
    block.vtx[0].rehash()
    block.hashMerkleRoot = block.calc_merkle_root()
    block.rehash()


def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(int(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r


# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase outputs will be P2PK outputs;
# otherwise anyone-can-spend outputs. The first output is the reward,
# which is not spendable for COINBASE_MATURITY blocks.
def create_coinbase(height, stake, snapshot_hash, pubkey = None, n_pieces = 1):
    assert(n_pieces > 0)
    stake_in = COutPoint(int(stake['txid'], 16), stake['vout'])
    coinbase = CTransaction()
    coinbase.set_type(TxType.COINBASE)
    script_sig = CScript([CScriptNum(height), ser_uint256(snapshot_hash)])
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff), script_sig, 0xffffffff))
    coinbase.vin.append(CTxIn(outpoint=stake_in, nSequence=0xffffffff))
    halvings = int(height/150) # regtest

    output_script = None
    if (pubkey != None):
        output_script = CScript([pubkey, OP_CHECKSIG])
    else:
        output_script = CScript([OP_TRUE])

    rewardoutput = CTxOut((50 * UNIT) >> halvings, output_script)

    piece_value = int(stake['amount'] * UNIT / n_pieces)
    outputs = [CTxOut(piece_value, output_script) for _ in range(n_pieces)]

    # Add the remainder to the first stake output
    # Do not add it to reward, as the reward output has to be exactly block reward + fees
    outputs[0].nValue += int(stake['amount'] * UNIT) - piece_value * n_pieces

    coinbase.vout = [ rewardoutput ] + outputs
    coinbase.calc_sha256()
    return coinbase


# Convenience wrapper
# Returns the signed coinbase
def sign_coinbase(node, coinbase):
    coinbase = sign_transaction(node, coinbase)
    coinbase.rehash()
    return coinbase


def generate(node, n, preserve_utxos=[]):
    """ Generate n blocks on the node, making sure not to touch the utxos specified.

    :param preserve_utxos: an iterable of either dicts {'txid': ..., 'vout': ...}
    """
    preserve_utxos = set((x['txid'], x['vout']) for x in preserve_utxos)

    snapshot_meta = get_tip_snapshot_meta(node)
    height = node.getblockcount()
    tip = int(node.getbestblockhash(), 16)
    block_time = node.getblock(hex(tip))['time'] + 1
    txouts = []

    for _ in range(n):
        if not txouts:
            txouts = [x for x in node.listunspent()
                      if (x['txid'], x['vout']) not in preserve_utxos]
        stake = txouts.pop()
        coinbase = sign_coinbase(node, create_coinbase(height, stake, snapshot_meta.hash))
        block = create_block(tip, coinbase, block_time)
        snapshot_meta = update_snapshot_with_tx(node, snapshot_meta.data, 0, height + 1, coinbase)
        block.solve()
        node.p2p.send_message(msg_block(block))
        tip = block.sha256
        block_time += 1
        height += 1


# Create a transaction.
# If the scriptPubKey is not specified, make it anyone-can-spend.
def create_transaction(prevtx, n, sig, value, scriptPubKey=CScript()):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value, scriptPubKey))
    tx.calc_sha256()
    return tx

def get_legacy_sigopcount_block(block, fAccurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, fAccurate)
    return count

def get_legacy_sigopcount_tx(tx, fAccurate=True):
    count = 0
    for i in tx.vout:
        count += i.scriptPubKey.GetSigOpCount(fAccurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the moment
        count += CScript(j.scriptSig).GetSigOpCount(fAccurate)
    return count

# Create a scriptPubKey corresponding to either a P2WPKH output for the
# given pubkey, or a P2WSH output of a 1-of-1 multisig for the given
# pubkey. Returns the hex encoding of the scriptPubKey.
def witness_script(use_p2wsh, pubkey):
    if (use_p2wsh == False):
        # P2WPKH instead
        pubkeyhash = hash160(hex_str_to_bytes(pubkey))
        pkscript = CScript([OP_0, pubkeyhash])
    else:
        # 1-of-1 multisig
        witness_program = CScript([OP_1, hex_str_to_bytes(pubkey), OP_1, OP_CHECKMULTISIG])
        scripthash = sha256(witness_program)
        pkscript = CScript([OP_0, scripthash])
    return bytes_to_hex_str(pkscript)

# Return a transaction (in hex) that spends the given utxo to a segwit output,
# optionally wrapping the segwit output using P2SH.
def create_witness_tx(node, use_p2wsh, utxo, pubkey, encode_p2sh, amount):
    if use_p2wsh:
        program = CScript([OP_1, hex_str_to_bytes(pubkey), OP_1, OP_CHECKMULTISIG])
        addr = script_to_p2sh_p2wsh(program) if encode_p2sh else script_to_p2wsh(program)
    else:
        addr = key_to_p2sh_p2wpkh(pubkey) if encode_p2sh else key_to_p2wpkh(pubkey)
    if not encode_p2sh:
        assert_equal(node.validateaddress(addr)['scriptPubKey'], witness_script(use_p2wsh, pubkey))
    return node.createrawtransaction([utxo], {addr: amount})

# Create a transaction spending a given utxo to a segwit output corresponding
# to the given pubkey: use_p2wsh determines whether to use P2WPKH or P2WSH;
# encode_p2sh determines whether to wrap in P2SH.
# sign=True will have the given node sign the transaction.
# insert_redeem_script will be added to the scriptSig, if given.
def send_to_witness(use_p2wsh, node, utxo, pubkey, encode_p2sh, amount, sign=True, insert_redeem_script=""):
    tx_to_witness = create_witness_tx(node, use_p2wsh, utxo, pubkey, encode_p2sh, amount)
    if (sign):
        signed = node.signrawtransaction(tx_to_witness)
        assert("errors" not in signed or len(["errors"]) == 0)
        return node.sendrawtransaction(signed["hex"])
    else:
        if (insert_redeem_script):
            tx = FromHex(CTransaction(), tx_to_witness)
            tx.vin[0].scriptSig += CScript([hex_str_to_bytes(insert_redeem_script)])
            tx_to_witness = ToHex(tx)

    return node.sendrawtransaction(tx_to_witness)


class SnapshotMeta:
    def __init__(self, res):
        self.hash = uint256_from_str(hex_str_to_bytes(res['hash']))
        self.data = res['data']


def get_tip_snapshot_meta(node):
    return SnapshotMeta(node.gettipsnapshot())


def calc_snapshot_hash(node, snapshot_data, stake_modifier, height, inputs, outputs):
    chain_work = 2 + 2*height
    res = node.calcsnapshothash(
        bytes_to_hex_str(ser_vector(inputs)),
        bytes_to_hex_str(ser_vector(outputs)),
        bytes_to_hex_str(ser_uint256(stake_modifier)),
        bytes_to_hex_str(ser_uint256(chain_work)),
        snapshot_data
    )
    return SnapshotMeta(res)


def update_snapshot_with_tx(node, snapshot_data, stake_modifier, height, tx):
    """
    Returns updated snapshot for a single tx (if need arises, change it to a list of txses)
    """

    is_coinbase = tx.get_type() == TxType.COINBASE.name
    vin_start = 1 if is_coinbase else 0

    node_height = node.getblockcount()

    inputs = []
    outputs = []

    for i in range(vin_start, len(tx.vin)):
        tx_in = tx.vin[i]
        prevout = node.gettxout(hex(tx_in.prevout.hash), tx_in.prevout.n)
        ctx_out = CTxOut(int(prevout['value']*UNIT), CScript(hex_str_to_bytes(prevout['scriptPubKey']['hex'])))
        utxo = UTXO(node_height + 1 - prevout['confirmations'], prevout['coinbase'], tx_in.prevout, ctx_out)
        inputs.append(utxo)

    for i, tx_out in enumerate(tx.vout):
        utxo = UTXO(height, is_coinbase, COutPoint(tx.sha256, i), tx_out)
        outputs.append(utxo)

    return calc_snapshot_hash(node, snapshot_data, stake_modifier, height, inputs, outputs)
