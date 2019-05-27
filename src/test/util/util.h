// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UTIL_H
#define UNIT_E_TEST_UTIL_H

#include <blockchain/blockchain_behavior.h>
#include <blockchain/blockchain_types.h>
#include <key.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <pubkey.h>

struct KeyFixture {
  std::unique_ptr<blockchain::Behavior> blockchain_behavior;
  CExtKey ext_key;
  CPubKey pub_key;
  std::vector<unsigned char> pub_key_data;
};

KeyFixture MakeKeyFixture(const std::string &seed_words = "cook note face vicious suggest company unit smart lobster tongue dune diamond faculty solid thought");

CTransactionRef MakeCoinbaseTransaction(const KeyFixture &key_fixture = MakeKeyFixture(), const blockchain::Height height = 0);

//! \brief creates a minimal block that passes validation without looking at the chain
CBlock MinimalBlock(const KeyFixture &key_fixture = MakeKeyFixture());

//! \brief creates a minimal block + extra data
//!
//! The first argument is a function which can be used to manipulate
//! the block before calculating merkle trees and block signature.
//! This is handy for testing: You can create blocks with a certain number
//! of transactions, blocks with invalid payloads, etc.
CBlock MinimalBlock(const std::function<void(CBlock &)>,
                    const KeyFixture &key_fixture = MakeKeyFixture());

#endif  //UNIT_E_TEST_UTIL_H
