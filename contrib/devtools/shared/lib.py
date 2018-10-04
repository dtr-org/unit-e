#!/usr/bin/env python3
# Copyright (c) 2018 UnitE Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os
import re
import subprocess
import sys

def changeto(dir = "."):
  os.chdir(os.path.dirname(os.path.realpath(__file__)))
  while not os.path.isdir(".git") and not os.path.isdir("src"):
    previous_dir = os.getcwd()
    os.chdir("..")
    if previous_dir == os.getcwd():
      print("Did not find root of git repository")
      sys.exit(1)
  os.chdir(dir)

def listfiles(glob = "*"):
  ps = subprocess.Popen("git ls-files '*'", shell=True, stdout=subprocess.PIPE)
  lines = ps.stdout.read().decode("utf-8")
  return list(filter(None, lines.split("\n")))

def checkfiles(action, pattern = None, glob = "*", dir = ".", invert = False):
  workingdirectory = os.getcwd()
  try:
    changeto(dir)
    fileset = listfiles()
    violations = []
    for filename in fileset:
      if pattern:
        matches = re.match(pattern, filename)
        if bool(matches) == invert:
          continue
      if not action(filename):
        violations.append(filename)
    return violations
  finally:
    os.chdir(workingdirectory) # restory working directory

