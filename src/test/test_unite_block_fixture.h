// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_TEST_TEST_UNITE_BLOCK_FIXTURE_H
#define UNITE_TEST_TEST_UNITE_BLOCK_FIXTURE_H

#include <primitives/block.h>

struct RealBlockFixture {

  const CBlock block;

  RealBlockFixture();
};

#endif  // UNITE_TEST_TEST_UNITE_BLOCK_FIXTURE_H
