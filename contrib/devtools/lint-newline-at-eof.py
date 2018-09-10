#!/usr/bin/env python3
# Copyright (c) 2016-2017 UnitE Core Developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import os
import re
import subprocess
import sys

exclude_files_pattern = "^.+\\.(bmp|ico|icns|json|pgp|png|raw|svg|ts)|(src/(leveldb|univalue|secp256k1|crypto/ctaes)||depends/patches)/.+$"

os.chdir(os.path.dirname(os.path.realpath(__file__)))
while not os.path.isdir(".git") and not os.path.isdir("src"):
    previous_dir = os.getcwd()
    os.chdir("..")
    if previous_dir == os.getcwd():
        print("Did not find root of git repository")
        sys.exit(1)

files = subprocess.Popen("git ls-files '*'", shell=True, stdout=subprocess.PIPE).stdout.read().decode("utf-8")

count_violations = 0

for filename in files.split("\n"):
    if not filename:
        continue
    if re.match(exclude_files_pattern, filename):
        continue
    with open(filename, 'rb') as file:
        try:
            file.seek(-1, os.SEEK_END)
            last_character = file.read(1)
            if last_character == b'\n':
                continue
        except OSError:
            continue
    print("{0} does not end in a newline (found {1} instead)".format(filename, last_character))
    count_violations += 1

sys.exit(1 if count_violations > 0 else 0)

