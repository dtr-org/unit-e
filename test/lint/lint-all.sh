#!/usr/bin/env bash
#
# Copyright (c) 2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# This script runs all contrib/devtools/lint-*.sh files, and fails if any exit
# with a non-zero status code.

# This script is intentionally locale dependent by not setting "export LC_ALL=C"
# in order to allow for the executed lint scripts to opt in or opt out of locale
# dependence themselves.

set -u

SCRIPTDIR=$(dirname "${BASH_SOURCE[0]}")
LINTALL=$(basename "${BASH_SOURCE[0]}")

SHELL_SCRIPTS=(
  lint-circular-dependencies.sh
  lint-clang-format.py
  lint-filenames.sh
  lint-format-strings.sh
  lint-includes.sh
  lint-locale-dependence.sh
  lint-logs.sh
  lint-newline-at-eof.py
  lint-python.sh
  lint-python-shebang.sh
  lint-python-utf8-encoding.sh
  lint-shebang.sh
  lint-shell-locale.sh
  lint-whitespace.sh
)

EXIT_CODE=0
for f in ${SHELL_SCRIPTS[@]}; do
  if ! "${SCRIPTDIR}/$f"; then
    echo "^---- failure generated from $f"
    EXIT_CODE=1
  fi
done

exit ${EXIT_CODE}
