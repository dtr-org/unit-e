#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


TRAVIS_COMMIT_LOG=$(git log --format=fuller -1)
export TRAVIS_COMMIT_LOG

if [ -n "$USE_SHELL" ]; then
  export CONFIG_SHELL="$USE_SHELL"
fi

OUTDIR=$BASE_OUTDIR/$TRAVIS_PULL_REQUEST/$TRAVIS_JOB_NUMBER-$HOST
UNITE_CONFIG_ALL="--disable-dependency-tracking --prefix=$TRAVIS_BUILD_DIR/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"

if [ -z "$NO_DEPENDS" ]; then
  depends/$HOST/native/bin/ccache --max-size=$CCACHE_SIZE
fi

test -n "$USE_SHELL" && eval '"$USE_SHELL" -c "./autogen.sh"' || ./autogen.sh

mkdir build && cd build

../configure --cache-file=config.cache $UNITE_CONFIG_ALL $UNITE_CONFIG || ( cat config.log && false)

make distdir VERSION=$HOST

cd unit-e-$HOST

./configure --cache-file=../config.cache $UNITE_CONFIG_ALL $UNITE_CONFIG || ( cat config.log && false)

make $MAKEJOBS $GOAL || ( echo "Build failure. Verbose build follows." && make $GOAL V=1 ; false )

export LD_LIBRARY_PATH=$TRAVIS_BUILD_DIR/depends/$HOST/lib

if [ "$RUN_TESTS" = "true" ]; then
  while sleep 9m; do echo "=====[ $SECONDS seconds still running ]====="; done &
  make $MAKEJOBS check VERBOSE=1
  kill %1
fi

if [ "$TRAVIS_EVENT_TYPE" = "cron" ]; then
  extended="--extended --exclude feature_pruning,feature_dbcrash"
fi

if [ "$RUN_TESTS" = "true" ]; then
  test/functional/test_runner.py --combinedlogslen=4000 --coverage --quiet ${extended}
fi
