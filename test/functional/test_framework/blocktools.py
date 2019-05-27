#!/usr/bin/env python3
# Copyright (c) 2015-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Utilities for manipulating blocks and transactions."""

from .address import (
    key_to_p2sh_p2wpkh,
    key_to_p2wpkh,
    script_to_p2sh_p2wsh,
    script_to_p2wsh,
)
from .messages import (
    CBlock,
    UNIT,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    FromHex,
    ToHex,
    TxType,
    UTXO,
    msg_block,
    ser_uint256,
    ser_vector,
    sha256,
    uint256_from_str,
)
from .script import (
    CScript,
    CScriptNum,
    OP_0,
    OP_1,
    OP_CHECKMULTISIG,
    OP_CHECKSIG,
    OP_TRUE,
    hash160,
)
from .util import (
    assert_equal,
    bytes_to_hex_str,
    hex_str_to_bytes,
)
from .test_framework import PROPOSER_REWARD
from io import BytesIO

MAX_BLOCK_SIGOPS = 20000

# Genesis block time (regtest)
TIME_GENESIS_BLOCK = 1296688602

# From BIP141
WITNESS_COMMITMENT_HEADER = b"\xaa\x21\xa9\xed"


def create_block(hashprev, coinbase, ntime=None, *, version=1):
    """Create a block (with regtest difficulty)."""
    block = CBlock()
    block.nVersion = version
    if ntime is None:
        import time
        block.nTime = int(time.time() + 600)
    else:
        block.nTime = ntime
    block.hashPrevBlock = hashprev
    block.nBits = 0x207fffff  # difficulty retargeting is disabled in REGTEST chainparams
    block.vtx.append(coinbase)
    block.compute_merkle_trees()
    block.calc_sha256()
    return block


def sign_transaction(node, tx):
    signresult = node.signrawtransactionwithwallet(bytes_to_hex_str(tx.serialize()))
    tx = CTransaction()
    f = BytesIO(hex_str_to_bytes(signresult['hex']))
    tx.deserialize(f)
    return tx

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

def create_coinbase(height, stake, snapshot_hash, pubkey=None, raw_script_pubkey=None, n_pieces=1):
    """Create a coinbase transaction, assuming no miner fees.

    If pubkey is passed in, the coinbase outputs will be P2PK outputs;
    otherwise anyone-can-spend outputs. The first output is the reward,
    which is not spendable for COINBASE_MATURITY blocks."""
    assert n_pieces > 0
    stake_in = COutPoint(int(stake['txid'], 16), stake['vout'])
    coinbase = CTransaction()
    coinbase.set_type(TxType.COINBASE)
    script_sig = CScript([CScriptNum(height), ser_uint256(snapshot_hash)])
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff), script_sig, 0xffffffff))
    coinbase.vin.append(CTxIn(outpoint=stake_in, nSequence=0xffffffff))

    output_script = None
    if (pubkey != None):
        output_script = CScript([pubkey, OP_CHECKSIG])
    elif raw_script_pubkey is not None:
        output_script = CScript(raw_script_pubkey)
    else:
        output_script = CScript([OP_TRUE])

    rewardoutput = CTxOut(int(PROPOSER_REWARD * UNIT), output_script)

    piece_value = int(stake['amount'] * UNIT / n_pieces)
    outputs = [CTxOut(piece_value, output_script) for _ in range(n_pieces)]

    # Add the remainder to the first stake output
    # Do not add it to reward, as the reward output has to be exactly block reward + fees
    outputs[0].nValue += int(stake['amount'] * UNIT) - piece_value * n_pieces

    coinbase.vout = [ rewardoutput ] + outputs
    coinbase.rehash()
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
        snapshot_meta = update_snapshot_with_tx(node, snapshot_meta, height + 1, coinbase)
        block.solve()
        node.p2p.send_message(msg_block(block))
        tip = block.sha256
        block_time += 1
        height += 1


def create_tx_with_script(prevtx, n, script_sig=b"", *, amount, script_pub_key=CScript()):
    """Return one-input, one-output transaction object
       spending the prevtx's n-th output with the given amount.

       Can optionally pass scriptPubKey and scriptSig, default is anyone-can-spend output.
    """
    tx = CTransaction()
    assert n < len(prevtx.vout)
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), script_sig, 0xffffffff))
    tx.vout.append(CTxOut(amount, script_pub_key))
    tx.calc_sha256()
    return tx

def create_transaction(node, txid, to_address, *, amount):
    """ Return signed transaction spending the first output of the
        input txid. Note that the node must be able to sign for the
        output that is being spent, and the node must not be running
        multiple wallets.
    """
    raw_tx = create_raw_transaction(node, txid, to_address, amount=amount)
    tx = CTransaction()
    tx.deserialize(BytesIO(hex_str_to_bytes(raw_tx)))
    return tx

def create_raw_transaction(node, txid, to_address, *, amount):
    """ Return raw signed transaction spending the first output of the
        input txid. Note that the node must be able to sign for the
        output that is being spent, and the node must not be running
        multiple wallets.
    """
    rawtx = node.createrawtransaction(inputs=[{"txid": txid, "vout": 0}], outputs={to_address: amount})
    signresult = node.signrawtransactionwithwallet(rawtx)
    assert_equal(signresult["complete"], True)
    return signresult['hex']

def get_legacy_sigopcount_block(block, accurate=True):
    count = 0
    for tx in block.vtx:
        count += get_legacy_sigopcount_tx(tx, accurate)
    return count

def get_legacy_sigopcount_tx(tx, accurate=True):
    count = 0
    for i in tx.vout:
        scriptpubkey = i.scriptPubKey
        if not isinstance(scriptpubkey, CScript):
            scriptpubkey = CScript(scriptpubkey)
        count += scriptpubkey.GetSigOpCount(accurate)
    for j in tx.vin:
        # scriptSig might be of type bytes, so convert to CScript for the moment
        count += CScript(j.scriptSig).GetSigOpCount(accurate)
    return count

def witness_script(use_p2wsh, pubkey):
    """Create a scriptPubKey for a pay-to-witness TxOut.

    This is either a P2WPKH output for the given pubkey, or a P2WSH output of a
    1-of-1 multisig for the given pubkey. Returns the hex encoding of the
    scriptPubKey."""
    if not use_p2wsh:
        # P2WPKH instead
        pubkeyhash = hash160(hex_str_to_bytes(pubkey))
        pkscript = CScript([OP_0, pubkeyhash])
    else:
        # 1-of-1 multisig
        witness_program = CScript([OP_1, hex_str_to_bytes(pubkey), OP_1, OP_CHECKMULTISIG])
        scripthash = sha256(witness_program)
        pkscript = CScript([OP_0, scripthash])
    return bytes_to_hex_str(pkscript)

def create_witness_tx(node, use_p2wsh, utxo, pubkey, encode_p2sh, amount):
    """Return a transaction (in hex) that spends the given utxo to a segwit output.

    Optionally wrap the segwit output using P2SH."""
    if use_p2wsh:
        program = CScript([OP_1, hex_str_to_bytes(pubkey), OP_1, OP_CHECKMULTISIG])
        addr = script_to_p2sh_p2wsh(program) if encode_p2sh else script_to_p2wsh(program)
    else:
        addr = key_to_p2sh_p2wpkh(pubkey) if encode_p2sh else key_to_p2wpkh(pubkey)
    if not encode_p2sh:
        assert_equal(node.getaddressinfo(addr)['scriptPubKey'], witness_script(use_p2wsh, pubkey))
    return node.createrawtransaction([utxo], {addr: amount})

def send_to_witness(use_p2wsh, node, utxo, pubkey, encode_p2sh, amount, sign=True, insert_redeem_script=""):
    """Create a transaction spending a given utxo to a segwit output.

    The output corresponds to the given pubkey: use_p2wsh determines whether to
    use P2WPKH or P2WSH; encode_p2sh determines whether to wrap in P2SH.
    sign=True will have the given node sign the transaction.
    insert_redeem_script will be added to the scriptSig, if given."""
    tx_to_witness = create_witness_tx(node, use_p2wsh, utxo, pubkey, encode_p2sh, amount)
    if (sign):
        signed = node.signrawtransactionwithwallet(tx_to_witness)
        assert "errors" not in signed or len(["errors"]) == 0
        return node.sendrawtransaction(signed["hex"])
    else:
        if (insert_redeem_script):
            tx = FromHex(CTransaction(), tx_to_witness)
            tx.vin[0].scriptSig += CScript([hex_str_to_bytes(insert_redeem_script)])
            tx_to_witness = ToHex(tx)

    return node.sendrawtransaction(tx_to_witness)


class SnapshotMeta:
    def __init__(self, res, stake_modifier):
        self.hash = uint256_from_str(hex_str_to_bytes(res['hash']))
        self.data = res['data']
        self.stake_modifier = stake_modifier


def get_tip_snapshot_meta(node):
    height = node.getblockcount()
    stake_modifier = node.tracechain(start=height, length=1)['chain'][0]['stake_modifier']
    return SnapshotMeta(node.gettipsnapshot(), stake_modifier)


def calc_snapshot_hash(node, snapshot_meta, height, inputs, outputs, stake_prevout_txid=None):
    chain_work = 2 + 2*height
    if isinstance(stake_prevout_txid, CTransaction):
        if len(stake_prevout_txid.vin) < 2:
            stake_prevout_txid = None
    if stake_prevout_txid is None:
        stake_modifier = snapshot_meta.stake_modifier
    else:
        if isinstance(stake_prevout_txid, CTransaction):
            stake_prevout_txid = stake_prevout_txid.vin[1].prevout.hash
        if isinstance(stake_prevout_txid, int):
            stake_prevout_txid = "%064x" % stake_prevout_txid
        stake_modifier = node.calcstakemodifier(txid=stake_prevout_txid, prev=snapshot_meta.stake_modifier)
    res = node.calcsnapshothash(
        bytes_to_hex_str(ser_vector(inputs)),
        bytes_to_hex_str(ser_vector(outputs)),
        stake_modifier,
        bytes_to_hex_str(ser_uint256(chain_work)),
        snapshot_meta.data
    )
    return SnapshotMeta(res, stake_modifier)


def update_snapshot_with_tx(node, snapshot_meta, height, tx):
    """
    Returns updated snapshot for a single tx (if need arises, change it to a list of txses)
    """

    is_coinbase = tx.get_type() == TxType.COINBASE
    vin_start = 1 if is_coinbase else 0

    node_height = node.getblockcount()

    inputs = []
    outputs = []

    for i in range(vin_start, len(tx.vin)):
        tx_in = tx.vin[i]
        prevout = node.gettxout(hex(tx_in.prevout.hash), tx_in.prevout.n)
        ctx_out = CTxOut(int(prevout['value']*UNIT), CScript(hex_str_to_bytes(prevout['scriptPubKey']['hex'])))
        utxo = UTXO(node_height + 1 - prevout['confirmations'], TxType[prevout['txtype']], tx_in.prevout, ctx_out)
        inputs.append(utxo)

    for i, tx_out in enumerate(tx.vout):
        utxo = UTXO(height, tx.get_type(), COutPoint(tx.sha256, i), tx_out)
        outputs.append(utxo)

    return calc_snapshot_hash(node, snapshot_meta, height, inputs, outputs, tx.vin[1].prevout.hash)
