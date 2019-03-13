// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/coin.h>

#include <util.h>

#include <chain.h>
#include <tinyformat.h>

namespace staking {

std::string Coin::ToString() const {
  return tfm::format("Coin(txid=%s,index=%d,amount=%d,height=%d)",
                     util::to_string(GetTransactionHash()), GetOutputIndex(), GetAmount(), GetHeight());
}

}  // namespace staking
