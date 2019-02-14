#!/usr/bin/env bash

# usage:
# - run contrib/devtools/run-functional-tests.sh from the repository root

NUM_CORES=$(python -c 'import multiprocessing; print (max(4, multiprocessing.cpu_count() // 3 * 2))')

test/functional/test_runner.py -j ${NUM_CORES}

