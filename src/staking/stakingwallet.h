// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_STAKINGWALLET_H
#define UNIT_E_STAKING_STAKINGWALLET_H

#include <amount.h>

#include <vector>

class COutput;
class CKey;
struct CMutableTransaction;

namespace staking {

//! \brief wallet functionality to support staking
//!
//! This class is an interface.
class StakingWallet {

 public:
  virtual CAmount GetStakeableBalance() const = 0;

  virtual void AvailableCoinsForStaking(std::vector<::COutput> &vCoins) = 0;

  virtual bool CreateCoinStake(unsigned int nBits, int64_t nTime,
                               int nBlockHeight, int64_t nFees,
                               ::CMutableTransaction &txNew,
                               ::CKey &keyOut) = 0;

  virtual ~StakingWallet() = default;
};

}  // namespace staking

#endif  // UNIT_E_STAKINGWALLET_H
