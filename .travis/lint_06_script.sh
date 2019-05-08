#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

(cd contrib; pytest)

if [ "$TRAVIS_EVENT_TYPE" = "pull_request" ]; then
  test/lint/commit-script-check.sh $TRAVIS_COMMIT_RANGE
fi

test/lint/git-subtree-check.sh src/crypto/ctaes || true
test/lint/git-subtree-check.sh src/secp256k1 || true
test/lint/git-subtree-check.sh src/univalue || true
test/lint/git-subtree-check.sh src/leveldb || true
test/lint/check-doc.py
test/lint/check-rpc-mappings.py .
test/lint/lint-all.sh || true
