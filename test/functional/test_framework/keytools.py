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
        if isinstance(data, str):
            data = bytes.fromhex(data)
        elif isinstance(data, array):
            data = bytes(list(data))
        elif isinstance(data, bytearray):
            data = bytes(data)
        elif isinstance(data, list):
            data = bytes(data)
        elif not isinstance(data, bytes):
            raise TypeError(
                'data is not of type bytes (is: %s) '
                'and could not be converted into bytes' % type(data))

        assert(isinstance(data, bytes))
        self.data = data

    def __iter__(self):
        return self.data.__init__()

    def __getitem__(self, item):
        return self.data.__getitem__(item)

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
        for char in deque(reversed(self.data)):
            acc += x * char
            x = x << 8
        result = b''
        while acc:
            acc, idx = divmod(acc, base)
            result = alphabet[idx:idx+1] + result
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

    def to_base58check(self, version=0x00):
        """Encode the data using base58check. Returns a str."""
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
    """A private key (like CKey)"""

    __slots__ = ['key', 'compressed']

    def __init__(self, data=None, compressed=True):
        if data is None:
            data = urandom(32)
        super(PrivateKey, self).__init__(data)
        length = len(self.data)
        if length != 32:
            raise ValueError(
                'key data is expected to be 32 bytes but was %d' % len(self.data))
        self.compressed = bool(compressed)
        self.key = CECKey()
        self.key.set_compressed(compressed=self.compressed)
        self.key.set_secretbytes(secret=self.data)

    def get_pubkey(self):
        """Derive a public key from this private key."""
        return PublicKey(data=self.key.get_pubkey())

    def legacy_address(self, version=0x6F):
        """Derive a legacy (base58check encoded) address from this private key."""
        return self.get_pubkey().legacy_address(version=version)

    def bech32_address(self, hrp="uert"):
        if not self.compressed:
            return PrivateKey(self.data, compressed=True).bech32_address(hrp)
        return self.get_pubkey().bech32_address(hrp=hrp)

    def sign(self, hash):
        return self.key.sign(hash)

    def wif(self, version=0xEF):
        """Dump the key in wallet import format."""
        data = self.data
        if self.compressed:
            data += bytes([0x01])
        return BinaryData(data).to_base58check(version=version)

    @classmethod
    def from_wif(cls, data):
        bin_data = cls.from_base58check(data).data
        compressed = len(bin_data) == 33 and bin_data[32] == 1
        if compressed:
            bin_data = bin_data[0:-1]
        return PrivateKey(data=bin_data, compressed=compressed)

    @classmethod
    def from_node(cls, node, address):
        wif = node.dumpprivkey(address=address)
        return cls.from_wif(wif)

    def to_node(self, node, version):
        node.importprivkey(privkey=self.wif(version=version))


class PublicKey(BinaryData):
    """A public key (like CPubKey)"""

    def __init__(self, data):
        length = len(data)
        if length != 33 and length != 66:
            raise ValueError('not a valid pubkey, size=%d' % length)
        super(PublicKey, self).__init__(data)

    def legacy_address(self, version):
        """Serialize this public key using base58check encoding."""
        h = BinaryData(ripemd160(sha256(self.data)))
        return h.to_base58check(version=version)

    def bech32_address(self, hrp="uert"):
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
                 human_readable_prefix, node=None):
        self.pubkey_version_byte = pubkey_version_byte
        self.privkey_version_byte = privkey_version_byte
        self.human_readable_prefix = human_readable_prefix
        self.node = node

    @staticmethod
    def for_node(node):
        chainparams = node.getchainparams()
        base58_prefixes = chainparams['base58_prefixes']
        pubkey_version_byte = base58_prefixes['PUBKEY_ADDRESS'][0]
        privkey_version_byte = base58_prefixes['SECRET_KEY'][0]
        return KeyTool(pubkey_version_byte, privkey_version_byte,
                       chainparams['bech32_human_readable_prefix'], node)

    @staticmethod
    def make_privkey(data=None):
        return PrivateKey(data=data)

    @staticmethod
    def make_pubkey(data=None):
        return PublicKey(data=data)

    def get_pubkey(self, address=None, node=None):
        if node is None:
            if self.node is None:
                raise ValueError('no node given, and no node initialized')
            return self.get_pubkey(node=self.node, address=address)
        if address is None:
            address = self.get_bech32_address(key_or_node=node)
        return PublicKey.from_node(node=node, address=address)

    def get_privkey(self, address=None, node=None):
        if node is None:
            if self.node is None:
                raise ValueError('no node given, and no node initialized')
            return self.get_privkey(node=self.node, address=address)
        if address is None:
            address = self.get_bech32_address(key_or_node=node)
        return PrivateKey.from_node(node=node, address=address)

    def get_bech32_address(self, key_or_node=None):
        if key_or_node is None:
            if self.node is None:
                raise ValueError('no key_or_node given, and no node initialized')
            return self.get_bech32_address(key_or_node=self.node)
        if isinstance(key_or_node, (PublicKey, PrivateKey)):
            return key_or_node.bech32_address(hrp=self.human_readable_prefix)
        return key_or_node.getnewaddress('', 'bech32')

    def get_legacy_address(self, key_or_node=None):
        if key_or_node is None:
            if self.node is None:
                raise ValueError('no key_or_node given, and no node initialized')
            return self.get_legacy_address(key_or_node=self.node)
        if isinstance(key_or_node, (PublicKey, PrivateKey)):
            return key_or_node.legacy_address(version=self.pubkey_version_byte)
        return key_or_node.getnewaddress('', 'legacy')

    def get_privkey_wif(self, key_or_node=None):
        if key_or_node is None:
            if self.node is None:
                raise ValueError('no key_or_node given, and no node initialized')
            return self.get_privkey_wif(key_or_node=self.node)
        if isinstance(key_or_node, PrivateKey):
            return key_or_node.wif(version=self.privkey_version_byte)
        address = self.get_bech32_address(key_or_node=key_or_node)
        self.get_privkey(address, key_or_node).wif(self.privkey_version_byte)
