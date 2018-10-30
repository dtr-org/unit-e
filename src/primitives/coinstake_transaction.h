// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_COINSTAKE_TRANSACTION_H
#define UNIT_E_COINSTAKE_TRANSACTION_H

#include <stdint.h>

#include <boost/optional.hpp>

#include <primitives/transaction.h>
#include <pubkey.h>
#include <uint256.h>

class CoinstakeTransaction {

 public:
  uint32_t height;

  uint256 utxoSetHash;

  CPubKey pubKey;

  static boost::optional<CTransaction> ReadTransaction(const CTransaction &);

};

#endif  // UNIT_E_COINSTAKE_TRANSACTION_H
