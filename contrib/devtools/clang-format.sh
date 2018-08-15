#!/usr/bin/env bash
#
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# This script runs all contrib/devtools/lint-*.sh files, and fails if any exit
# with a non-zero status code.

export LC_ALL=C.UTF-8

for file in $(git ls-files '*.h'); do
  if [[ "$file" =~ src/crypto/ctaes/.+ || "$file" =~ src/secp256k1/.+ || "$file" =~ src/univalue/.+ || "$file" =~ src/leveldb/.+ ]]; then
    echo "Excluding $file"
  else
    echo "Formatting $file..."
    clang-format -style=Google -i "$file"
  fi
done

for file in $(git ls-files '*.cpp'); do
  if [[ "$file" =~ src/crypto/ctaes/.+ || "$file" =~ src/secp256k1/.+ || "$file" =~ src/univalue/.+ || "$file" =~ src/leveldb/.+ ]]; then
    echo "Excluding $file"
  else
    echo "Formatting $file..."
    clang-format -style=Google -i "$file"
  fi
done

