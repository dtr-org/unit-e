#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from .key import CECKey
from .messages import (
    hash256,
    ripemd160,
    sha256,
)
from .segwit_addr import (
    encode
)
from .test_node import (
    TestNode
)

from array import array
from collections import deque
from os import urandom

BASE16_ALPHABET = b'0123456789ABCDEF'
BECH32_ALPHABET = b'qpzry9x8gf2tvdw0s3jn54khce6mua7l'
BASE58_ALPHABET = b'123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'
BASE64_ALPHABET = b'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/'


class BinaryData:
    __slots__ = ['data']

    def __init__(self, data):
        """
        Initializes a new BinaryData object from the given data.

        :param data: required, may be any of the following types: BinaryData, str, array, bytearray, list, bytes. If
            given as a string the string is interpreted as the hexadecimal representation of binary data. It does not
            matter whether such a string starts with "0x" or not.

        :raises TypeError: if the given data is not of any of the acceptable types.
        """
        if isinstance(data, BinaryData):
            data = data.data
        elif isinstance(data, str):
            if data[0:2] == "0x":
                data = data[2:]
            data = bytes.fromhex(data)
        elif isinstance(data, array):
            data = bytes(list(data))
        elif isinstance(data, (bytearray, list)):
            data = bytes(data)
        elif not isinstance(data, bytes):
            raise TypeError(
                'data is not of type bytes (is: %s) '
                'and could not be converted into bytes' % type(data))

        assert isinstance(data, bytes)
        self.data = data

    def __iter__(self):
        return self.data.__iter__()

    def __getitem__(self, item):
        return self.data.__getitem__(item)

    def __len__(self):
        return len(self.data)

    def __bytes__(self):
        return self.data

    def __int__(self):
        return int.from_bytes(self.data, byteorder='big')

    def __str__(self):
        return self.to_hex()

    def __repr__(self):
        return "BinaryData(%s)" % str(self)

    def __eq__(self, other):
        if not isinstance(other, BinaryData):
            other = BinaryData(other)
        return self.data == other.data

    def to_hex(self):
        return self.encode(alphabet=BASE16_ALPHABET)

    def to_list(self):
        return list(self.data)

    def to_array(self):
        return array('B', self.to_list())

    def to_bytes(self):
        return self.data

    def to_bytearray(self):
        return bytearray(self.data)

    def encode(self, alphabet):
        base = len(alphabet)
        x, acc = 1, 0
        for char in reversed(self.data):
            acc += x * char
            x = x << 8
        result = b''
        while acc:
            acc, idx = divmod(acc, base)
            result = alphabet[idx:idx + 1] + result
        return result.decode('ascii')

    @staticmethod
    def decode(data, alphabet, length=0):
        if isinstance(data, str):
            data = bytes(data, 'ascii')
        base = len(alphabet)
        num = 0
        for char in data:
            num = num * base + alphabet.index(char)
        result = deque()
        while num > 0:
            num, mod = divmod(num, 256)
            result.appendleft(mod)
        if length == 0:
            padding = b''
        else:
            padding = b'\0' * (length - len(result))
        return BinaryData(data=padding + bytes(result))

    def to_base58check(self, version):
        """
        Encodes the data using base58check. Returns a str.

        The Base58Check encoding scheme is described in https://en.bitcoin.it/wiki/Base58Check_encoding.

        :param version: The version byte to use. 0x00 for mainnet, 0xEF for testnet/regtest.
        """
        data = bytes([version]) + self.data
        checksum = hash256(data)[0:4]
        result = BinaryData(data + checksum).encode(alphabet=BASE58_ALPHABET)
        padding = 0
        for char in data:
            if char == 0:
                padding += 1
            else:
                break
        return '1' * padding + result

    @classmethod
    def from_base58check(cls, data):
        return BinaryData(cls.decode(data=data, alphabet=BASE58_ALPHABET)[1:-4])


class PrivateKey(BinaryData):
    """
    A private key (like CKey in C++ code)

    A private key is a 256-bit number represented by a string of 32 bytes.
    A private key can be used to derive a public key from it, using `get_pubkey()`.
    """

    __slots__ = ['key', 'compressed']

    def __init__(self, data=None, compressed=True):
        """
        Constructs a private key.

        :param data: optional, defaults to None. If omitted, a random private key will be generated. May be of any type
            that is accepted by BinaryData.
        :param compressed: optional, defaults to True. Whether a public key derived from this private key will be
            compressed or not.

        :raises ValueError: in case data is not None and does not comprise 32 bytes.
        """
        if data is None:
            data = urandom(32)
        super(PrivateKey, self).__init__(data)
        length = len(self.data)
        if length != 32:
            raise ValueError('key data is expected to be 32 bytes but was %d' % len(self.data))
        self.compressed = bool(compressed)
        self.key = CECKey()
        self.key.set_compressed(compressed=self.compressed)
        self.key.set_secretbytes(secret=self.data)

    def __repr__(self):
        return "PrivateKey(%s)" % str(self)

    def get_pubkey(self):
        """ Derive a public key from this private key. """
        return PublicKey(data=self.key.get_pubkey())

    def legacy_address(self, version):
        """ Derive a legacy (base58check encoded) address from this private key. """
        return self.get_pubkey().legacy_address(version=version)

    def bech32_address(self, hrp):
        """
        Derives a bech32 address from this private key.

        Bech32 addresses are the native segwit format, see: https://github.com/bitcoin/bips/blob/master/bip-0173.mediawiki

        :param hrp: The human readable prefix to use for the address.
        :return: A P2PWKH bech32 encoded address. Funds spent to this address can be unlocked using this private key.
        """
        if not self.compressed:
            return PrivateKey(self.data, compressed=True).bech32_address(hrp)
        return self.get_pubkey().bech32_address(hrp=hrp)

    def sign(self, hash):
        return self.key.sign(hash)

    def wif(self, version):
        """
        Dump the key in wallet import format (WIF).

        Wallet Import Format is described at https://en.bitcoin.it/wiki/Wallet_import_format
        Test vectors can be obtained via http://gobittest.appspot.com/PrivateKey

        :param version The version byte to be used for this address. 0xEF is the version byte for regtest. Using
            KeyTool the chainparams in use will be populated automatically from the node.
        """
        data = self.data
        if self.compressed:
            data += bytes([0x01])
        return BinaryData(data).to_base58check(version=version)

    @classmethod
    def from_wif(cls, data):
        """
        Import a private key from a string given in wallet import format (WIF).

        :param data: The key given in WIF. The wallet import format is a base58check encoded string of bytes.
        :return: The private key obtained from the given WIF serialized string.

        :raises TypeError: in case the given data is not a str.
        """
        if not isinstance(data, str):
            raise TypeError('data is expected to be str, but was %s' % type(data))

        bin_data = cls.from_base58check(data).data
        compressed = len(bin_data) == 33 and bin_data[32] == 1
        if compressed:
            bin_data = bin_data[0:-1]
        return PrivateKey(data=bin_data, compressed=compressed)

    @classmethod
    def from_node(cls, node, address):
        wif = node.dumpprivkey(address=address)
        return cls.from_wif(wif)

    def to_node(self, node, version, rescan=True):
        node.importprivkey(privkey=self.wif(version=version), label="", rescan=rescan)


class PublicKey(BinaryData):
    """
    A public key (like CPubKey in C++ code)

    A public key is represented by a string of bytes of 33 or 66 bytes length.
    The 33 bytes version is called a "compressed public key", the 66 bytes version
    is an "uncompressed public key".
    """

    def __init__(self, data):
        length = len(data)
        if length != 33 and length != 66:
            raise ValueError('not a valid pubkey, size=%d' % length)
        super(PublicKey, self).__init__(data)

    def __repr__(self):
        return "PublicKey(%s)" % str(self)

    def legacy_address(self, version):
        """
        Creates a P2PKH address in base58check encoding from this public key.

        An in-depth explanation of the technical background of legacy addresses can be found at:
        - https://en.bitcoin.it/wiki/Technical_background_of_version_1_Bitcoin_addresses
        - http://royalforkblog.github.io/2014/08/11/graphical-address-generator/
        """
        h = BinaryData(ripemd160(sha256(self.data)))
        return h.to_base58check(version=version)

    def bech32_address(self, hrp):
        """
        Derives a bech32 address from this public key.

        Bech32 addresses are the native segwit format, see: https://github.com/bitcoin/bips/blob/master/bip-0173.mediawiki

        :param hrp: The human readable prefix to use for the address.
        :return: A P2PWKH bech32 encoded address.
        """
        if not self.is_compressed():
            raise ValueError('not a compressed pubkey')
        return encode(hrp, 0, ripemd160(sha256(self.data)))

    def is_compressed(self):
        return len(self.data) == 33

    @classmethod
    def from_node(cls, node, address):
        return PrivateKey.from_node(node, address).get_pubkey()

    def to_node(self, node, rescan=True):
        node.importpubkey(pubkey=self.to_hex(), label="", rescan=rescan)


class KeyTool(object):
    __slots__ = [
        'node',
        'pubkey_version_byte',
        'privkey_version_byte',
        'human_readable_prefix',
    ]

    def __init__(self,
                 pubkey_version_byte, privkey_version_byte,
                 human_readable_prefix, node):

        if not isinstance(node, TestNode):
            raise TypeError('node is not an instance of TestNode (is: %s)' % type(node))

        self.pubkey_version_byte = pubkey_version_byte
        self.privkey_version_byte = privkey_version_byte
        self.human_readable_prefix = human_readable_prefix
        self.node = node

    @staticmethod
    def for_node(node):
        """
        Creates a KeyTool for use with the given node.

        The node will be queried for the chainparams in use and the version bytes and human readable prefixes
        will be used automatically when creating addresses. This allows tests to be written agnostic from the
        chainparams/network in use.

        :param node: A TestNode instance.
        :return: A new KeyTool instance.
        """
        chainparams = node.getchainparams()
        base58_prefixes = chainparams['base58_prefixes']
        pubkey_version_byte = base58_prefixes['PUBKEY_ADDRESS'][0]
        privkey_version_byte = base58_prefixes['SECRET_KEY'][0]
        return KeyTool(pubkey_version_byte, privkey_version_byte,
                       chainparams['bech32_human_readable_prefix'], node)

    @staticmethod
    def make_privkey(data=None):
        """
        Creates a new private key.

        This function does not communicate with a node. The key has to be sent to the node using KeyTool.upload_key
        if it should be used with the node.

        :param data: optional, defaults to None. If omitted, a random private key will be generated. May be of any type
            that is accepted by BinaryData.__init__
        """
        return PrivateKey(data=data)

    @staticmethod
    def make_pubkey(data=None):
        """
        Creates a new public key.

        This function does not communicate with a node. The key has to be sent to the node using KeyTool.upload_key
        if it should be used with the node.

        :param data: optional, defaults to None. If omitted, a random public key will be generated. May be of any type
            that is accepted by BinaryData.__init__
        """
        return PublicKey(data=data)

    def upload_key(self, key, rescan=True, node=None):
        """
        Uploads the given key using `importprivkey` or `importpubkey` into a node's wallet.

        :param key: An instance of a PublicKey or PrivateKey.
        :param rescan: Whether the node's wallet should rescan transactions using the imported key.
        :param node: optional, defaults to None. The node to import this key at. If None given the node which this
            KeyTool was initialized with is used.
        """
        if node is None:
            node = self.node
        if isinstance(key, PrivateKey):
            key.to_node(node=node, version=self.privkey_version_byte, rescan=rescan)
        elif isinstance(key, PublicKey):
            key.to_node(node=node, rescan=rescan)

    def get_pubkey(self, address, node=None):
        """
        Looks up the public key associated with the given address from a node.

        :param address: A bech32 or legacy address given as a string.
        :param node: optional, defaults to None. The node to lookup the key at. If None given the node which this
            KeyTool was initialized with is used.
        """
        if node is None:
            node = self.node
        return PublicKey.from_node(node=node, address=address)

    def get_privkey(self, address, node=None):
        """
        Looks up the private key associated with the given address from a node.

        :param address: A bech32 or legacy address given as a string.
        :param node: optional, defaults to None. The node to lookup the key at. If None given the node which this
            KeyTool was initialized with is used.
        """
        if node is None:
            node = self.node
        return PrivateKey.from_node(node=node, address=address)

    def get_bech32_address(self, key_or_node=None):
        """
        Get a bech32/segwit address for a given key or from a node.

        Bech32 addresses are the native segwit format, see: https://github.com/bitcoin/bips/blob/master/bip-0173.mediawiki

        :param key_or_node: optional, defaults to None. Either a key (instance of PrivateKey or PublicKey) or a
            TestNode. If a key is given the address is derived from that key and no communication with any node
            is done. Otherwise the given node (or if None: the node with which this KeyTool instance was initialized
            with) will be queried for a new address using `getnewaddress`.
        :return: A bech32 encoded address as a string.
        """
        if key_or_node is None:
            key_or_node = self.node
        if isinstance(key_or_node, (PublicKey, PrivateKey)):
            return key_or_node.bech32_address(hrp=self.human_readable_prefix)
        return key_or_node.getnewaddress('', 'bech32')

    def get_legacy_address(self, key_or_node=None):
        """
        Get a legacy address for a given key or from a node.

        :param key_or_node: optional, defaults to None. Either a key (instance of PrivateKey or PublicKey) or a
            TestNode. If a key is given the address is derived from that key and no communication with any node
            is done. Otherwise the given node (or if None: the node with which this KeyTool instance was initialized
            with) will be queried for a new address using `getnewaddress`.
        :return: A legacy address encoded using base58check as a string.
        """
        if key_or_node is None:
            key_or_node = self.node
        if isinstance(key_or_node, (PublicKey, PrivateKey)):
            return key_or_node.legacy_address(version=self.pubkey_version_byte)
        return key_or_node.getnewaddress('', 'legacy')

    def get_privkey_wif(self, key):
        """
        Export a private key in wallet import format (WIF).

        Wallet Import Format is described at https://en.bitcoin.it/wiki/Wallet_import_format
        Test vectors can be obtained via http://gobittest.appspot.com/PrivateKey

        Uses the version bytes as reported by the chainparams from the node with which this KeyTool instance was
        initialized with.

        :param key: The private key to serialize in wallet import format (WIF).
        :raises TypeError: If key is not a PrivateKey
        """
        if not isinstance(key, PrivateKey):
            raise TypeError('not a PrivateKey given, but: %s' % type(key))
        return key.wif(version=self.privkey_version_byte)
