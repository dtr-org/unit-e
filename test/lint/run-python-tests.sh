#!/usr/bin/env bash
#
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Run unit tests for Python scripts in contrib/

failed=false

run_unit_test() {
  echo "Running unit test $1..."
  if ! python3 $1; then
    failed=true
  fi
}

run_unit_test contrib/devtools/test_copyright_header.py
run_unit_test contrib/devtools/test_camel_to_snake.py

if $failed; then
  echo run-python-tests FAILED
  exit 1
else
  echo run-python-tests PASSED
fi
