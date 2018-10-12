#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

./contrib/devtools/git-subtree-check.sh src/crypto/ctaes
# disabled as we cherry-picked the following commits:
# https://github.com/Bitcoin-ABC/bitcoin-abc/commit/f4f00f4ed342f12774a5e8da006a20193e325008
# https://github.com/Bitcoin-ABC/bitcoin-abc/commit/77507ac86f977832f03ec274eb64328fb9126f6c
# ./contrib/devtools/git-subtree-check.sh src/secp256k1
./contrib/devtools/git-subtree-check.sh src/univalue
./contrib/devtools/git-subtree-check.sh src/leveldb
./contrib/devtools/check-doc.py
./contrib/devtools/check-rpc-mappings.py .
./contrib/devtools/lint-all.sh

