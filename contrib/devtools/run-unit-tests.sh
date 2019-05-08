#!/usr/bin/env bash

# tested on macOS:
# - requires GNU Parallel (`brew install parallel`)

# tested on ubuntu:
# - requires GNU Parallel (`sudo apt-get install parallel`)

# usage:
# - run contrib/devtools/run-unit-tests.sh from the repository root

export LC_ALL=C

which parallel > /dev/null || (echo "GNU parallel is not installed"; false) || exit 1

src/test/test_unite --list_content 2>&1 | \
  grep -v -F '    ' | \
  tr -d '*' | \
  awk '{ f = $0 ".log"; print "src/test/test_unite --run_test=" $0 " > \"" f "\" 2>&1 && (echo \"- [x] " $0 "\"; rm \"" f "\"; true) || (echo \"- [ ] " $0 " (see " f ")\"; false)"}' | \
  parallel -j 0 bash -c 2> /dev/null | \
  # Sort test results alphabetically, with the failing ones grouped at the bottom
  sort -t / -k 1.6 | sort -s -t / -k 1.3,1.5 -r
