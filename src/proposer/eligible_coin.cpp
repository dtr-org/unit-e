// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/eligible_coin.h>

#include <tinyformat.h>

namespace proposer {

std::string EligibleCoin::ToString() const {
  return tfm::format(
      "tx=%s, index=%d, amount=%d, kernel=%d, depth=%d, "
      "target_height=%d, target_time=%d, target_difficulty=%d",
      stake.hash.GetHex(),
      stake.n,
      amount,
      kernel_hash.GetHex(),
      depth,
      target_height,
      target_time,
      target_difficulty);
}

}  // namespace proposer
