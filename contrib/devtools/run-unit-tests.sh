#!/usr/bin/env bash

# tested on macOS:
# - requires GNU Parallel (`brew install parallel`)

# tested on ubuntu:
# - requires GNU Parallel (`sudo apt-get install parallel`)

# usage:
# - run contrib/devtools/run-unit-tests.sh from the repository root

src/test/test_unite --list_content 2>&1 | \
  grep -v -F '    ' | \
  awk '{ print "src/test/test_unite --run_test=" $0 " > /dev/null 2>&1 && echo - [x] " $0 " || echo - [ ] " $0 }' | \
  parallel -j 0 bash -c 2> /dev/null | \
  sort
