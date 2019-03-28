// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_ELIGIBLE_COIN_H
#define UNIT_E_PROPOSER_ELIGIBLE_COIN_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <primitives/transaction.h>
#include <staking/coin.h>
#include <uint256.h>

#include <string>

namespace proposer {

//! \brief A coin that can be used as Proof-of-Stake when proposing.
struct EligibleCoin {
  //! The unspent transaction output which is currently eligible to be used as stake.
  staking::Coin utxo;

  //! The kernel hash that was computed for the block using this coin.
  uint256 kernel_hash;

  //! The reward associated with this coin, not including fees.
  CAmount reward;

  //! The height at which the newly proposed block will be at. This is one more
  //! then the currently active chain's height.
  blockchain::Height target_height;

  //! The time that was used to check the kernel and which will be set as the
  //! time of the newly proposed block.
  blockchain::Time target_time;

  //! The difficulty that was used to check the kernel and which will be set as
  //! the difficulty of the newly proposed block (nBits).
  blockchain::Difficulty target_difficulty;

  std::string ToString() const;
};

}  // namespace proposer

#endif  //UNIT_E_PROPOSER_ELIGIBLE_COIN_H
