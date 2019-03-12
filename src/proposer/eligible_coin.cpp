// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/eligible_coin.h>

#include <util.h>

#include <tinyformat.h>

namespace proposer {

std::string EligibleCoin::ToString() const {
  return tfm::format(
      "tx=%s, index=%d, amount=%d, height=%d, kernel=%d, "
      "target_height=%d, target_time=%d, target_difficulty=%d",
      util::to_string(utxo.GetTransactionHash()),
      utxo.GetOutputIndex(),
      utxo.GetAmount(),
      utxo.GetHeight(),
      util::to_string(kernel_hash),
      target_height,
      target_time,
      target_difficulty);
}

}  // namespace proposer
