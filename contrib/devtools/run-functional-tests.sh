#!/usr/bin/env bash

# usage:
# - run contrib/devtools/run-functional-tests.sh from the repository root

NUM_CORES=$(python -c 'import multiprocessing; print (multiprocessing.cpu_count())')

test/functional/test_runner.py -j ${NUM_CORES}

