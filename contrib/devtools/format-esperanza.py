#!/usr/bin/env python3
# Copyright (c) 2016-2017 UnitE Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import subprocess
import sys

import shared.lib

def formatcpp(filename):
  subprocess.call(["clang-format", "-i", "-style=file", filename])
  return True

violations = shared.lib.checkfiles(
        pattern = ".+\\.(cpp|h)",
        dir = "src/esperanza",
        action = formatcpp)

sys.exit(0)

