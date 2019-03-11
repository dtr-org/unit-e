// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/coin.h>

#include <util.h>

#include <tinyformat.h>

namespace staking {

Coin::Coin(const uint256 &txid,
           const std::uint32_t index,
           const CAmount amount,
           const CScript &script_pubkey,
           const blockchain::Depth depth)
    : txid(txid), index(index), amount(amount), script_pubkey(script_pubkey), depth(depth) {}

std::string Coin::ToString() const {
  return tfm::format("Coin(txid=%s,index=%d,amount=%d,depth=%d)", util::to_string(txid), index, amount, depth);
}

}  // namespace staking
