#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

./contrib/devtools/git-subtree-check.sh src/crypto/ctaes
./contrib/devtools/git-subtree-check.sh src/secp256k1
./contrib/devtools/git-subtree-check.sh src/univalue
./contrib/devtools/git-subtree-check.sh src/leveldb
./contrib/devtools/check-doc.py
./contrib/devtools/check-rpc-mappings.py .
./contrib/devtools/lint-all.sh

