#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test calcsnapshothash RPC"""
from test_framework.test_framework import UnitETestFramework
from test_framework.util import (
    assert_equal,
    hex_str_to_bytes
)

from test_framework.messages import (
    ser_vector,
    bytes_to_hex_str,
    uint256_from_str,
    UTXO,
    COutPoint,
    CTxOut,
    ser_uint256,
)


class RpcCalcSnapshotHashTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        def assert_not_equal(thing1, thing2, *args):
            if thing1 == thing2 or any(thing1 == arg for arg in args):
                raise AssertionError("equal(%s)" % " != ".join(str(arg) for arg in (thing1, thing2) + args))

        def ser_utxos(utxos):
            return bytes_to_hex_str(ser_vector(utxos))

        def calcsnapshothash(inputs, outputs, *prev_hash):
            sm = bytes_to_hex_str(ser_uint256(0))
            return self.nodes[0].calcsnapshothash(ser_utxos(inputs), ser_utxos(outputs), sm, *prev_hash)

        def def_utxo(height):
            hex_id = hex_str_to_bytes('0' * 64)
            uint256 = uint256_from_str(hex_id)
            return UTXO(height, 0, COutPoint(uint256, 0), CTxOut(0, b""))

        # empty inputs and outputs doesn't produce empty hash
        # as it includes stake modifier
        empty = calcsnapshothash([], [])
        assert_equal(len(empty['hash']), 64)
        assert_equal(len(empty['data']), 192)
        assert_not_equal(empty['hash'], '0' * 64)
        assert_equal(empty['data'], '0' * 192)

        # test hash generation
        one = calcsnapshothash([], [def_utxo(0)])
        assert_not_equal(one, empty)

        two = calcsnapshothash([], [def_utxo(0), def_utxo(1)])
        assert_not_equal(two['hash'], one['hash'])
        assert_not_equal(two['hash'], empty['hash'])

        # test addition
        assert_equal(
            calcsnapshothash([], [def_utxo(1)], one['data'])['hash'],
            two['hash']
        )

        # test subtraction
        assert_equal(
            calcsnapshothash([def_utxo(1)], [], two['data'])['hash'],
            one['hash']
        )
        assert_equal(
            calcsnapshothash([def_utxo(0), def_utxo(1)], [], two['data'])['hash'],
            empty['hash']
        )

        # test update
        three = calcsnapshothash([], [def_utxo(0), def_utxo(1), def_utxo(2)])
        assert_equal(
            calcsnapshothash([def_utxo(0)], [], three['data'])['hash'],
            calcsnapshothash([], [def_utxo(1), def_utxo(2)])['hash']
        )


if __name__ == '__main__':
    RpcCalcSnapshotHashTest().main()
