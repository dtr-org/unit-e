#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

TRAVIS_COMMIT_LOG=$(git log --format=fuller -1)
export TRAVIS_COMMIT_LOG

if [ -n "$USE_SHELL" ]; then
  export CONFIG_SHELL="$USE_SHELL"
fi

OUTDIR=$BASE_OUTDIR/$TRAVIS_PULL_REQUEST/$TRAVIS_JOB_NUMBER-$HOST
UNITE_CONFIG_ALL="--disable-dependency-tracking --prefix=$TRAVIS_BUILD_DIR/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"

if [ -z "$NO_DEPENDS" ]; then
  DOCKER_EXEC ccache --max-size=$CCACHE_SIZE
fi

BEGIN_FOLD autogen
if [ -n "$CONFIG_SHELL" ] ; then
  DOCKER_EXEC "$CONFIG_SHELL" -c "./autogen.sh"
else
  DOCKER_EXEC ./autogen.sh
fi
END_FOLD

mkdir build && cd build

BEGIN_FOLD configure
DOCKER_EXEC ../configure --cache-file=config.cache $UNITE_CONFIG_ALL $UNITE_CONFIG || ( cat config.log && false)
END_FOLD

BEGIN_FOLD distdir
DOCKER_EXEC make distdir VERSION=$HOST
END_FOLD

cd unit-e-$HOST

BEGIN_FOLD configure
DOCKER_EXEC ./configure --cache-file=../config.cache $UNITE_CONFIG_ALL $UNITE_CONFIG || ( cat config.log && false)
END_FOLD

BEGIN_FOLD build
DOCKER_EXEC make $MAKEJOBS $GOAL || ( echo "Build failure. Verbose build follows." && DOCKER_EXEC make $GOAL V=1 ; false )
END_FOLD

if [ "$RUN_TESTS" = "true" ]; then
  BEGIN_FOLD unit-tests
  echo "Running the unit tests (this might take a while...)"
  while sleep 9m; do echo "=====[ $SECONDS seconds still running ]====="; done &
  DOCKER_EXEC LD_LIBRARY_PATH=$TRAVIS_BUILD_DIR/depends/$HOST/lib make $MAKEJOBS check VERBOSE=1
  kill %1
  END_FOLD
fi

if [ "$RUN_BENCH" = "true" ]; then
  BEGIN_FOLD bench
  DOCKER_EXEC LD_LIBRARY_PATH=$TRAVIS_BUILD_DIR/depends/$HOST/lib $OUTDIR/bin/bench_unite -scaling=0.001
  END_FOLD
fi

if [ "$TRAVIS_EVENT_TYPE" = "cron" ]; then
  extended="--extended --exclude feature_pruning,feature_dbcrash"
fi

if [ "$RUN_TESTS" = "true" ]; then
  BEGIN_FOLD functional-tests
  DOCKER_EXEC test/functional/test_runner.py --combinedlogslen=4000 --coverage --quiet --failfast ${extended}
  END_FOLD
fi
