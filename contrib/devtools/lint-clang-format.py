#!/usr/bin/env python3
# Copyright (c) 2016-2017 UnitE Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import subprocess
import sys

import shared.lib

def checkfile(filename):
  ps = subprocess.Popen("clang-format -style=file " + filename,
                        shell=True,
                        stdout=subprocess.PIPE)
  formatted = ps.stdout.read().decode("utf-8")
  with open(filename, "rb") as file:
    unformatted = file.read().decode("utf-8")
  isformatted = formatted == unformatted
  if not isformatted:
    print(filename, "is not formatted")
  return isformatted

violations = shared.lib.checkfiles(
        pattern = ".+\\.(cpp|h)",
        dir = "src/esperanza",
        action = checkfile)

sys.exit(1 if len(violations) > 0 else 0)
