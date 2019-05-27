#!/usr/bin/env python3
# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os
import sys

import shared.lib

def checkfile(filename):
  with open(filename, 'rb') as file:
    try:
      file.seek(-1, os.SEEK_END)
      lastcharacter = file.read(1)
      if lastcharacter == b'\n':
        return True
    except OSError as err:
      return True
  print(filename, "does not end in a newline")
  return False

violations = shared.lib.checkfiles(
        pattern = "^.+\\.(bmp|ico|icns|json|pgp|png|raw|svg|ts)|(src/(leveldb|univalue|secp256k1|crypto/ctaes)|depends/patches)/.+$",
        invert = True,
        action = checkfile)

sys.exit(1 if len(violations) > 0 else 0)
