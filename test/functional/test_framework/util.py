#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Helpful routines for regression testing."""

from base64 import b64encode
from binascii import hexlify, unhexlify
from decimal import Decimal, ROUND_DOWN
import hashlib
import inspect
import json
import logging
import os
import random
import re
from subprocess import CalledProcessError
import time
import types
import math

from . import coverage
from .authproxy import AuthServiceProxy, JSONRPCException

logger = logging.getLogger("TestFramework.utils")

# Assert functions
##################

def assert_fee_amount(fee, tx_size, fee_per_kB):
    """Assert the fee was in range"""
    target_fee = round(tx_size * fee_per_kB / 1000, 8)
    if fee < target_fee:
        raise AssertionError("Fee of %s UTE too low! (Should be %s UTE)" % (str(fee), str(target_fee)))
    # allow the wallet's estimation to be at most 2 bytes off
    if fee > (tx_size + 2) * fee_per_kB / 1000:
        raise AssertionError("Fee of %s UTE too high! (Should be %s UTE)" % (str(fee), str(target_fee)))

def assert_equal(thing1, thing2, *args):
    if thing1 != thing2 or any(thing1 != arg for arg in args):
        raise AssertionError("not(%s)" % " == ".join(str(arg) for arg in (thing1, thing2) + args))

def assert_not_equal(thing, *args):
    if any(thing == arg for arg in args):
        raise AssertionError("expected not equal(%s)" % ", ".join(str(arg) for arg in (thing,) + args))

def assert_greater_than(thing1, thing2):
    if thing1 <= thing2:
        raise AssertionError("%s <= %s" % (str(thing1), str(thing2)))

def assert_greater_than_or_equal(thing1, thing2):
    if thing1 < thing2:
        raise AssertionError("%s < %s" % (str(thing1), str(thing2)))

def assert_less_than(thing1, thing2):
    if thing1 >= thing2:
        raise AssertionError("%s >= %s" % (str(thing1), str(thing2)))

def assert_less_than_or_equal(thing1, thing2):
    if thing1 > thing2:
        raise AssertionError("%s > %s" % (str(thing1), str(thing2)))

def assert_in(thing, sequence):
    if thing not in sequence:
        raise AssertionError("%s not in %s" % (str(thing), str(sequence)))

def assert_contents_equal(thing1, thing2, key=lambda x: x):
    if len(thing1) != len(thing2):
        raise AssertionError("Sequences have different lengths: {} and {}".format(thing1, thing2))
    assert_equal(sorted(thing1, key=key), sorted(thing2, key=key))

def assert_raises(exc, fun, *args, **kwds):
    assert_raises_message(exc, None, fun, *args, **kwds)

def assert_raises_message(exc, message, fun, *args, **kwds):
    try:
        fun(*args, **kwds)
    except JSONRPCException:
        raise AssertionError("Use assert_raises_rpc_error() to test RPC failures")
    except exc as e:
        if message is not None and message not in e.error['message']:
            raise AssertionError("Expected substring not found:" + e.error['message'])
    except Exception as e:
        raise AssertionError("Unexpected exception raised: " + type(e).__name__)
    else:
        raise AssertionError("No exception raised")

def assert_raises_process_error(returncode, output, fun, *args, **kwds):
    """Execute a process and asserts the process return code and output.

    Calls function `fun` with arguments `args` and `kwds`. Catches a CalledProcessError
    and verifies that the return code and output are as expected. Throws AssertionError if
    no CalledProcessError was raised or if the return code and output are not as expected.

    Args:
        returncode (int): the process return code.
        output (string): [a substring of] the process output.
        fun (function): the function to call. This should execute a process.
        args*: positional arguments for the function.
        kwds**: named arguments for the function.
    """
    try:
        fun(*args, **kwds)
    except CalledProcessError as e:
        if returncode != e.returncode:
            raise AssertionError("Unexpected returncode %i" % e.returncode)
        if output not in e.output:
            raise AssertionError("Expected substring not found:" + e.output)
    else:
        raise AssertionError("No exception raised")

def assert_raises_rpc_error(code, message, fun, *args, **kwds):
    """Run an RPC and verify that a specific JSONRPC exception code and message is raised.

    Calls function `fun` with arguments `args` and `kwds`. Catches a JSONRPCException
    and verifies that the error code and message are as expected. Throws AssertionError if
    no JSONRPCException was raised or if the error code/message are not as expected.

    Args:
        code (int), optional: the error code returned by the RPC call (defined
            in src/rpc/protocol.h). Set to None if checking the error code is not required.
        message (string), optional: [a substring of] the error string returned by the
            RPC call. Set to None if checking the error string is not required.
        fun (function): the function to call. This should be the name of an RPC.
        args*: positional arguments for the function.
        kwds**: named arguments for the function.
    """
    assert try_rpc(code, message, fun, *args, **kwds), "No exception raised"

def assert_matches(actual, expected, strict=True, path=()):
    """
    Checks that a given value matches a given pattern.

    :param actual: The value to check.
    :param expected:
        When given a dict invokes assert_matches recursively for every item.
        Each key present in the dict for comparison (the expected value) must
        be present in the given dict. In strict mode all keys in the given value
        must have a definition in the expected kind.
        When given a list invokes assert_matches for every item.
        When given a string the value must equal that string.
        When given an int the value must equal that int.
        When given a type the value must be of that type.
        When given a function the value must satisfy that predicate.
    :param strict:
        In strict mode a dictionary must have exactly the same keys
        (otherwise at least the same keys) and lists must have the same length.
    """

    try:
        if type(expected) == dict:
            if type(actual) != dict:
                raise AssertionError("Structure does not match, expected dictionary but got %s" % type(actual))
            actual_keys = set(actual.keys())
            expected_keys = set(expected.keys())
            if strict:
                if actual_keys != expected_keys:
                    raise AssertionError("Structure does not match, expected keys %s but got %s" % (expected_keys, actual_keys))
            else:
                if not expected_keys <= actual_keys:
                    raise AssertionError("Structure does not match, expected %s to be a subset of %s" % (actual_keys, expected_keys))
            for key, value in expected.items():
                assert_matches(actual[key], value, strict=strict, path=path+(key,))
        elif type(expected) == list:
            if type(actual) != list:
                raise AssertionError("Structure does not match, expected a list but got %s" % type(actual))
            if strict:
                if len(actual) != len(expected):
                    raise AssertionError("Structure does not match, lists have different lengths, expected %s but got %s" % (len(expected), len(actual)))
            ix = 0
            for actual_item, expected_item in zip(actual, expected):
                assert_matches(actual_item, expected_item, strict=strict, path=path+(ix,))
                ix = ix + 1
        elif type(expected) == str:
            assert_equal(actual, expected)
        elif type(expected) == int:
            assert_equal(actual, expected)
        elif isinstance(expected, types.FunctionType):
            if not expected(actual, path):
                if expected.message:
                    raise AssertionError("Structure does not match: %s (%s)" % (actual, expected.message))
                raise AssertionError("Structure does not match: %s" % actual)
        else:
            assert_equal(type(actual), expected)
    except AssertionError:
        print("Pattern match error at %s" % (path,))
        raise

class Matcher:
    """
    Matchers allow you to match for certain values in assert_matches:

    assert_matches(some_value_you_want_to_check, {
        'foo': Matcher.match(lambda v: re.fullmatch(r'qu{1,2}x', v)),
        'bar': Matcher.many(str, min=3, max=10),
    });
    """

    @staticmethod
    def match(func, message=None):
        """A matcher that checks whether a value satisfies a given predicate."""
        def check(value, path=()):
            return func(value)
        if message:
            check.message = message
        else:
            check.message = inspect.getsource(func).strip()
        return check

    @classmethod
    def hexstr(cls, length):
        """Matches that a value is a hexadecimal string of the given length."""
        return cls.match(lambda v: len(v) == length and re.fullmatch(r'[a-fA-Z0-9]+', v))

    @classmethod
    def eq(cls, value):
        """Matches that a value equals the given reference value."""
        return cls.match(lambda v: v == value)

    @classmethod
    def many(cls, expected, min=None, max=None):
        """
        Matches that a given value conforms to the given kind, optionally
        minimum and/or maximum number of items.

        :param expected: A kind as expected by assert_matches
        :param min: (optional) minimum number of items in the list (incl.)
        :param max: (optional) maximum number of items in the list (incl.)
        """
        def check(values, path=()):
            if min:
                assert_greater_than_or_equal(len(values), min)
            if max:
                assert_less_than_or_equal(len(values), max)
            for value in values:
                assert_matches(value, expected, path=path)
            return True
        return cls.match(check)

def try_rpc(code, message, fun, *args, **kwds):
    """Tries to run an rpc command.

    Test against error code and message if the rpc fails.
    Returns whether a JSONRPCException was raised."""
    try:
        fun(*args, **kwds)
    except JSONRPCException as e:
        # JSONRPCException was thrown as expected. Check the code and message values are correct.
        if (code is not None) and (code != e.error["code"]):
            raise AssertionError("Unexpected JSONRPC error code %i" % e.error["code"])
        if (message is not None) and (message not in e.error['message']):
            raise AssertionError("Expected substring not found:" + e.error['message'])
        return True
    except Exception as e:
        raise AssertionError("Unexpected exception raised: " + type(e).__name__)
    else:
        return False

def assert_is_hex_string(string):
    try:
        int(string, 16)
    except Exception as e:
        raise AssertionError(
            "Couldn't interpret %r as hexadecimal; raised: %s" % (string, e))

def assert_is_hash_string(string, length=64):
    if not isinstance(string, str):
        raise AssertionError("Expected a string, got type %r" % type(string))
    elif length and len(string) != length:
        raise AssertionError(
            "String of length %d expected; got %d" % (length, len(string)))
    elif not re.match('[abcdef0-9]+$', string):
        raise AssertionError(
            "String %r contains invalid characters for a hash." % string)

def assert_array_result(object_array, to_match, expected, should_not_find=False):
    """
        Pass in array of JSON objects, a dictionary with key/value pairs
        to match against, and another dictionary with expected key/value
        pairs.
        If the should_not_find flag is true, to_match should not be found
        in object_array
        """
    if should_not_find:
        assert_equal(expected, {})
    num_matched = 0
    for item in object_array:
        all_match = True
        for key, value in to_match.items():
            if item[key] != value:
                all_match = False
        if not all_match:
            continue
        elif should_not_find:
            num_matched = num_matched + 1
        for key, value in expected.items():
            if item[key] != value:
                raise AssertionError("%s : expected %s=%s" % (str(item), str(key), str(value)))
            num_matched = num_matched + 1
    if num_matched == 0 and not should_not_find:
        raise AssertionError("No objects matched %s" % (str(to_match)))
    if num_matched > 0 and should_not_find:
        raise AssertionError("Objects were found %s" % (str(to_match)))


def assert_finalizationstate(node, expected):
    state = node.getfinalizationstate()
    for key in expected:
        a = state[key]
        b = expected[key]
        if a != b:
            raise AssertionError("%s: not(%s == %s)" % (str(key), str(a), str(b)))

# Utility functions
###################

def check_json_precision():
    """Make sure json library being used does not lose precision converting UTE values"""
    n = Decimal("20000000.00000003")
    satoshis = int(json.loads(json.dumps(float(n))) * 1.0e8)
    if satoshis != 2000000000000003:
        raise RuntimeError("JSON encode/decode loses precision")

def count_bytes(hex_string):
    return len(bytearray.fromhex(hex_string))

def bytes_to_hex_str(byte_str):
    return hexlify(byte_str).decode('ascii')

def hash256(byte_str):
    sha256 = hashlib.sha256()
    sha256.update(byte_str)
    sha256d = hashlib.sha256()
    sha256d.update(sha256.digest())
    return sha256d.digest()[::-1]

def hex_str_to_bytes(hex_str):
    return unhexlify(hex_str.encode('ascii'))

def str_to_b64str(string):
    return b64encode(string.encode('utf-8')).decode('ascii')

def satoshi_round(amount):
    return Decimal(amount).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)

def wait_until(predicate, *, attempts=float('inf'), timeout=float('inf'), lock=None):
    if attempts == float('inf') and timeout == float('inf'):
        timeout = 60
    attempt = 0
    time_end = time.time() + timeout

    while attempt < attempts and time.time() < time_end:
        if lock:
            with lock:
                if predicate():
                    return
        else:
            if predicate():
                return
        attempt += 1
        time.sleep(0.05)

    # Print the cause of the timeout
    predicate_source = inspect.getsourcelines(predicate)
    logger.error("wait_until() failed. Predicate: {}".format(predicate_source))
    if attempt >= attempts:
        raise AssertionError("Predicate {} not true after {} attempts".format(predicate_source, attempts))
    elif time.time() >= time_end:
        raise AssertionError("Predicate {} not true after {} seconds".format(predicate_source, timeout))
    raise RuntimeError('Unreachable')

# RPC/P2P connection constants and functions
############################################

# The maximum number of nodes a single test can spawn
MAX_NODES = 8
# Don't assign rpc or p2p ports lower than this
PORT_MIN = 11000
# The number of ports to "reserve" for p2p and rpc, each
PORT_RANGE = 5000

class PortSeed:
    # Must be initialized with a unique integer for each process
    n = None

def get_rpc_proxy(url, node_number, timeout=None, coveragedir=None):
    """
    Args:
        url (str): URL of the RPC server to call
        node_number (int): the node number (or id) that this calls to

    Kwargs:
        timeout (int): HTTP timeout in seconds

    Returns:
        AuthServiceProxy. convenience object for making RPC calls.

    """
    proxy_kwargs = {}
    if timeout is not None:
        proxy_kwargs['timeout'] = timeout

    proxy = AuthServiceProxy(url, **proxy_kwargs)
    proxy.url = url  # store URL on proxy for info

    coverage_logfile = coverage.get_filename(
        coveragedir, node_number) if coveragedir else None

    return coverage.AuthServiceProxyWrapper(proxy, coverage_logfile)

def p2p_port(n):
    assert n <= MAX_NODES
    return PORT_MIN + n + (MAX_NODES * PortSeed.n) % (PORT_RANGE - 1 - MAX_NODES)

def rpc_port(n):
    return PORT_MIN + PORT_RANGE + n + (MAX_NODES * PortSeed.n) % (PORT_RANGE - 1 - MAX_NODES)

def rpc_url(datadir, i, rpchost=None):
    rpc_u, rpc_p = get_auth_cookie(datadir)
    host = '127.0.0.1'
    port = rpc_port(i)
    if rpchost:
        parts = rpchost.split(':')
        if len(parts) == 2:
            host, port = parts
        else:
            host = rpchost
    return "http://%s:%s@%s:%d" % (rpc_u, rpc_p, host, int(port))

# Node functions
################

def initialize_datadir(dirname, n):
    datadir = os.path.join(dirname, "node" + str(n))
    if not os.path.isdir(datadir):
        os.makedirs(datadir)
    with open(os.path.join(datadir, "unit-e.conf"), 'w', encoding='utf8') as f:
        f.write("regtest=1\n")
        f.write("port=" + str(p2p_port(n)) + "\n")
        f.write("rpcport=" + str(rpc_port(n)) + "\n")
        f.write("listenonion=0\n")
    return datadir

def get_datadir_path(dirname, n):
    return os.path.join(dirname, "node" + str(n))

def get_auth_cookie(datadir):
    user = None
    password = None
    if os.path.isfile(os.path.join(datadir, "unit-e.conf")):
        with open(os.path.join(datadir, "unit-e.conf"), 'r', encoding='utf8') as f:
            for line in f:
                if line.startswith("rpcuser="):
                    assert user is None  # Ensure that there is only one rpcuser line
                    user = line.split("=")[1].strip("\n")
                if line.startswith("rpcpassword="):
                    assert password is None  # Ensure that there is only one rpcpassword line
                    password = line.split("=")[1].strip("\n")
    if os.path.isfile(os.path.join(datadir, "regtest", ".cookie")):
        with open(os.path.join(datadir, "regtest", ".cookie"), 'r') as f:
            userpass = f.read()
            split_userpass = userpass.split(':')
            user = split_userpass[0]
            password = split_userpass[1]
    if user is None or password is None:
        raise ValueError("No RPC credentials")
    return user, password

# If a cookie file exists in the given datadir, delete it.
def delete_cookie_file(datadir):
    if os.path.isfile(os.path.join(datadir, "regtest", ".cookie")):
        logger.debug("Deleting leftover cookie file")
        os.remove(os.path.join(datadir, "regtest", ".cookie"))

def get_bip9_status(node, key):
    info = node.getblockchaininfo()
    return info['bip9_softforks'][key]

def set_node_times(nodes, t):
    for node in nodes:
        node.setmocktime(t)

def disconnect_nodes(from_connection, node_num):
    for peer_id in [peer['id'] for peer in from_connection.getpeerinfo() if "testnode%d" % node_num in peer['subver']]:
        try:
            from_connection.disconnectnode(nodeid=peer_id)
        except JSONRPCException as e:
            # If this node is disconnected between calculating the peer id
            # and issuing the disconnect, don't worry about it.
            # This avoids a race condition if we're mass-disconnecting peers.
            if e.error['code'] != -29: # RPC_CLIENT_NODE_NOT_CONNECTED
                raise

    # wait to disconnect
    wait_until(lambda: [peer['id'] for peer in from_connection.getpeerinfo() if "testnode%d" % node_num in peer['subver']] == [], timeout=5)

def connect_nodes(from_connection, node_num):
    ip_port = "127.0.0.1:" + str(p2p_port(node_num))
    from_connection.addnode(ip_port, "onetry")
    # poll until version handshake complete to avoid race conditions
    # with transaction relaying
    wait_until(lambda:  all(peer['version'] != 0 for peer in from_connection.getpeerinfo()))

def connect_nodes_bi(nodes, a, b):
    connect_nodes(nodes[a], b)
    connect_nodes(nodes[b], a)

def sync_blocks(rpc_connections, *, wait=1, timeout=60, height=None):
    """
    Wait until everybody has the same tip.

    if height is not specified, then sync_blocks needs to be called with an
    rpc_connections set that has least one node already synced to the latest,
    stable tip, otherwise there's a chance it might return before all nodes are
    stably synced.
    """
    if height is None:
        # Use getblockcount() instead of waitforblockheight() to determine the
        # initial max height because the two RPCs look at different internal
        # global variables (chainActive vs latestBlock) and the former gets
        # updated earlier.
        maxheight = max(x.getblockcount() for x in rpc_connections)
    else:
        maxheight = height

    start_time = cur_time = time.time()
    while cur_time <= start_time + timeout:
        tips = [r.waitforblockheight(maxheight, int(wait * 1000)) for r in rpc_connections]
        if all(t["height"] == maxheight for t in tips):
            if all(t["hash"] == tips[0]["hash"] for t in tips):
                return
            raise AssertionError("Block sync failed, mismatched block hashes:{}".format(
                                 "".join("\n  {!r}".format(tip) for tip in tips)))
        cur_time = time.time()
    raise AssertionError("Block sync to height {} timed out:{}".format(
                         maxheight, "".join("\n  {!r}".format(tip) for tip in tips)))

def sync_chain(rpc_connections, *, wait=1, timeout=60):
    """
    Wait until everybody has the same best block
    """
    while timeout > 0:
        best_hash = [x.getbestblockhash() for x in rpc_connections]
        if best_hash == [best_hash[0]] * len(best_hash):
            return
        time.sleep(wait)
        timeout -= wait
    raise AssertionError("Chain sync failed: Best block hashes don't match")

def sync_mempools(rpc_connections, *, wait=1, timeout=150, flush_scheduler=True):
    """
    Wait until everybody has the same transactions in their memory
    pools
    """
    timeout += time.perf_counter()
    mempools = dict()

    while time.perf_counter() < timeout:
        sample_pool = set(rpc_connections[0].getrawmempool())
        mempools[rpc_connections[0].index] = sample_pool
        all_synced = True

        for rpc_connection in rpc_connections[1:]:
            pool = set(rpc_connection.getrawmempool())
            mempools[rpc_connection.index] = pool

            if pool != sample_pool:
                all_synced = False

        if all_synced:
            if flush_scheduler:
                for r in rpc_connections:
                    r.syncwithvalidationinterfacequeue()
            return

        time.sleep(wait)

    raise AssertionError("Mempool sync failed:%s" % "".join(
        ["\nNode %d: %s" % entry for entry in mempools.items()]))

def get_unspent_coins(node, n_coins, lock=False):
    """
    Wrapper for listing coins to use for staking.
    """
    unspent_outputs = node.listunspent()
    assert len(unspent_outputs) >= n_coins
    # return from the from to avoid problems on reorg
    if lock:
        node.lockunspent(False, [{'txid': tx['txid'], 'vout': tx['vout']} for tx in unspent_outputs[:n_coins]])
    return unspent_outputs[:n_coins]


# Transaction/Block functions
#############################

def find_output(node, txid, amount):
    """
    Return index to output of txid with value amount
    Raises exception if there is none.
    """
    txdata = node.getrawtransaction(txid, 1)
    for i in range(len(txdata["vout"])):
        if txdata["vout"][i]["value"] == amount:
            return i
    raise RuntimeError("find_output txid %s : %s not found" % (txid, str(amount)))

def gather_inputs(from_node, amount_needed, confirmations_required=1):
    """
    Return a random set of unspent txouts that are enough to pay amount_needed
    """
    assert confirmations_required >= 0
    utxo = from_node.listunspent(confirmations_required)
    random.shuffle(utxo)
    inputs = []
    total_in = Decimal("0.00000000")
    while total_in < amount_needed and len(utxo) > 0:
        t = utxo.pop()
        total_in += t["amount"]
        inputs.append({"txid": t["txid"], "vout": t["vout"], "address": t["address"]})
    if total_in < amount_needed:
        raise RuntimeError("Insufficient funds: need %d, have %d" % (amount_needed, total_in))
    return (total_in, inputs)

def make_change(from_node, amount_in, amount_out, fee):
    """
    Create change output(s), return them
    """
    outputs = {}
    amount = amount_out + fee
    change = amount_in - amount
    if change > amount * 2:
        # Create an extra change output to break up big inputs
        change_address = from_node.getnewaddress()
        # Split change in two, being careful of rounding:
        outputs[change_address] = Decimal(change / 2).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)
        change = amount_in - amount - outputs[change_address]
    if change > 0:
        outputs[from_node.getnewaddress()] = change
    return outputs

def random_transaction(nodes, amount, min_fee, fee_increment, fee_variants):
    """
    Create a random transaction.
    Returns (txid, hex-encoded-transaction-data, fee)
    """
    from_node = random.choice(nodes)
    to_node = random.choice(nodes)
    fee = min_fee + fee_increment * random.randint(0, fee_variants)

    (total_in, inputs) = gather_inputs(from_node, amount + fee)
    outputs = make_change(from_node, total_in, amount, fee)
    outputs[to_node.getnewaddress()] = float(amount)

    rawtx = from_node.createrawtransaction(inputs, outputs)
    signresult = from_node.signrawtransaction(rawtx)
    txid = from_node.sendrawtransaction(signresult["hex"], True)

    return (txid, signresult["hex"], fee)

# Helper to create at least "count" utxos
# Pass in a fee that is sufficient for relay and mining new transactions.
def create_confirmed_utxos(fee, node, count):
    to_generate = int(0.5 * count) + 101
    while to_generate > 0:
        node.generate(min(25, to_generate))
        to_generate -= 25
    utxos = node.listunspent()
    iterations = count - len(utxos)
    addr1 = node.getnewaddress("", "bech32")
    addr2 = node.getnewaddress("", "bech32")
    if iterations <= 0:
        return utxos
    for i in range(iterations):
        t = utxos.pop()
        inputs = []
        inputs.append({"txid": t["txid"], "vout": t["vout"]})
        outputs = {}
        send_value = t['amount'] - fee
        outputs[addr1] = satoshi_round(send_value / 2)
        outputs[addr2] = satoshi_round(send_value / 2)
        raw_tx = node.createrawtransaction(inputs, outputs)
        signed_tx = node.signrawtransaction(raw_tx)["hex"]
        node.sendrawtransaction(signed_tx)

    while (node.getmempoolinfo()['size'] > 0):
        node.generate(1)

    utxos = node.listunspent()
    assert len(utxos) >= count
    return utxos

# Create large OP_RETURN txouts that can be appended to a transaction
# to make it large (helper for constructing large transactions).
def gen_return_txouts():
    # Some pre-processing to create a bunch of OP_RETURN txouts to insert into transactions we create
    # So we have big transactions (and therefore can't fit very many into each block)
    # create one script_pubkey
    script_pubkey = "6a4d0200"  # OP_RETURN OP_PUSH2 512 bytes
    for i in range(512):
        script_pubkey = script_pubkey + "01"
    # concatenate 128 txouts of above script_pubkey which we'll insert before the txout for change
    txouts = "81"
    for k in range(128):
        # add txout value
        txouts = txouts + "0000000000000000"
        # add length of script_pubkey
        txouts = txouts + "fd0402"
        # add script_pubkey
        txouts = txouts + script_pubkey
    return txouts

def create_tx(node, coinbase, to_address, amount):
    inputs = [{"txid": coinbase, "vout": 0}]
    outputs = {to_address: amount}
    rawtx = node.createrawtransaction(inputs, outputs)
    signresult = node.signrawtransaction(rawtx)
    assert_equal(signresult["complete"], True)
    return signresult["hex"]

# Create a spend of each passed-in utxo, splicing in "txouts" to each raw
# transaction to make it large.  See gen_return_txouts() above.
def create_lots_of_big_transactions(node, txouts, utxos, num, fee):
    addr = node.getnewaddress()
    txids = []
    for _ in range(num):
        t = utxos.pop()
        inputs = [{"txid": t["txid"], "vout": t["vout"]}]
        outputs = {}
        change = t['amount'] - fee
        outputs[addr] = satoshi_round(change)
        rawtx = node.createrawtransaction(inputs, outputs)
        newtx = rawtx[0:92]
        newtx = newtx + txouts
        newtx = newtx + rawtx[94:]
        signresult = node.signrawtransaction(newtx, None, None, "NONE")
        txid = node.sendrawtransaction(signresult["hex"], True)
        txids.append(txid)
    return txids

def mine_large_block(node, utxos=None):
    # generate a 66k transaction,
    # and 14 of them is close to the 1MB block limit
    num = 14
    txouts = gen_return_txouts()
    utxos = utxos if utxos is not None else []

    # We must pass enough transactions
    assert len(utxos) >= num

    fee = 100 * node.getnetworkinfo()["relayfee"]
    create_lots_of_big_transactions(node, txouts, utxos, num, fee=fee)
    node.generate(1)

def base58_to_bytes(string):
    char_map = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
    decimal = 0
    for char in string:
        decimal = 58 * decimal + char_map.index(char)

    return decimal.to_bytes(math.ceil(math.log2(decimal)/8), byteorder='big')


def base58check_to_bytes(string):
    return base58_to_bytes(string)[1:-4]


def bytes_to_base58(bytes):
    char_map = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'

    i = int.from_bytes(bytes, byteorder='big')
    if i == 0:
        return char_map[0]

    string = ""
    while i > 0:
        i, idx = divmod(i, 58)
        string = char_map[idx] + string

    return string
