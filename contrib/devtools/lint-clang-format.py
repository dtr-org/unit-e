#!/usr/bin/env python3
# Copyright (c) 2018 UnitE Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os
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
  return isformatted

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

def git_changed_files():
  ps = subprocess.Popen("git diff --cached --name-status",
                        shell=True,
                        stdout=subprocess.PIPE)
  result = []
  for line in ps.stdout.read().decode("utf-8").splitlines():
    status, filename = line.split()
    if status in ["A", "M"]:
      result += [filename]
  return result

def is_unite(filename):
  dirs = [
    "src/esperanza",
    "src/proposer",
    "src/snapshot"
  ]
  for d in dirs:
    if filename.startswith(d):
      return True
  return False;

def filter_unite(files):
  return [f for f in files if is_unite(f)]


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
  if iscurrentcommit:
    violations = [f for f in filter_unite(git_changed_files()) if not checkfile(f)]
  else:
    for dir in dirs:
      dir_v = shared.lib.checkfiles(
        pattern = ".+\\.(cpp|h)",
        dir = dir,
        action = checkfile)
      for v in dir_v:
        violations += ["{}/{}".format(dir, v)]
  if len(violations) == 0:
    return 0
  print("Unformatted files:")
  for v in violations:
    print("*", v)
  if autoformat:
    ok = True
    for v in violations:
      ok = ok and formatfile(v) == 0
    if ok:
      print("... have been formatted.")
      if autogitadd:
        ok = True
        for v in violations:
          ok = ok and gitadd(v) == 0
        if ok:
          print("... and added back into commit.")
          return 0
      else:
        print("Now you can add it into your commit.")
  return 1


if __name__ == "__main__":
  sys.exit(main(sys.argv))
