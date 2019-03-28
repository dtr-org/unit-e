#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


if [ -n "$DPKG_ADD_ARCH" ]; then
  sudo dpkg --add-architecture "$DPKG_ADD_ARCH"
fi

if [ -n "$PACKAGES" ]; then
  travis_retry sudo apt-get update
fi

if [ -n "$PACKAGES" ]; then
  travis_retry sudo apt-get install --no-install-recommends --no-upgrade -qq $PACKAGES
fi
