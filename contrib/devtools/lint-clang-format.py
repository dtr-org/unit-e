#!/usr/bin/env python3
# Copyright (c) 2018 UnitE Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os
import subprocess
import sys

import shared.lib

def formatfile(filename):
  ps = subprocess.Popen("clang-format -style=file -i " + filename,
                        shell=True,
                        stdout=subprocess.PIPE)
  return ps.wait()

def gitadd(filename):
  ps = subprocess.Popen("git add " + filename,
                        shell=True,
                        stdout=subprocess.PIPE)
  return ps.wait()

def checkandupdate(filename, replace = False, addtogit = False):
  ps = subprocess.Popen("clang-format -style=file " + filename,
                        shell=True,
                        stdout=subprocess.PIPE)
  formatted = ps.stdout.read().decode("utf-8")
  with open(filename, "rb") as file:
    unformatted = file.read().decode("utf-8")
  isformatted = formatted == unformatted
  if not isformatted:
    if replace:
      if formatfile(filename) == 0:
        if addtogit:
          if gitadd(filename) == 0:
            print(filename, "has been formatted and added to commit")
            return True
        else:
          print(filename, "has been formatted")
    else:
      print(filename, "is not formatted")
  return isformatted

def help(argv):
  print("Using: {0} [--check-commit] [--replace [--git-add]]".format(argv[0]))
  print()
  print("Checking unit-e sources follow style guide.")
  print("With no options, just check all the project files.")
  print()
  print("--check-commit   consider only Unit-E files from the current commit")
  print("--replace        adjust unformatted files")
  print("--git-add        add formated files back into your commit")
  return 1

def main(argv):
  if ("-h" in argv) or ("--help" in argv):
    return help(argv);
  autoformat = "--replace" in argv
  autogitadd = autoformat and "--git-add" in argv
  iscurrentcommit = "--check-commit" in argv
  dirs = [
    "src/esperanza",
    "src/proposer",
    "src/snapshot"
  ]
  violations = []
  for dir in dirs:
    violations += shared.lib.checkfiles(
      pattern = ".+\\.(cpp|h)",
      dir = dir,
      action = lambda f : checkandupdate(f, replace=autoformat, addtogit=autogitadd),
      only_changed = iscurrentcommit)
  return 0 if len(violations) == 0 else 1

if __name__ == "__main__":
  sys.exit(main(sys.argv))
