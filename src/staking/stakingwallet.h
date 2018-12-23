// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_STAKINGWALLET_H
#define UNIT_E_STAKING_STAKINGWALLET_H

#include <amount.h>
#include <proposer/proposer_state.h>
#include <staking/coin.h>
#include <sync.h>

#include <vector>

class COutput;
class CKey;
struct CMutableTransaction;

namespace staking {

//! \brief Wallet functionality to support staking
//!
//! This class is an interface.
class StakingWallet {

 public:
  //! \brief access to the mutex that protects the active chain.
  //!
  //! Usage: LOCK(chain->GetLock())
  //!
  //! This way the existing DEBUG_LOCKORDER and other debugging features can
  //! work as expected.
  virtual CCriticalSection &GetLock() const = 0;

  //! \brief returns the reserve balance currently set.
  //!
  //! The proposer will always make sure that it does not use more than this
  //! amount for staking.
  virtual CAmount GetReserveBalance() const = 0;

  //! \brief returns the amount that can currently be used for staking.
  virtual CAmount GetStakeableBalance() const = 0;

  //! \brief returns the coins that can currently be used for staking.
  virtual std::vector<staking::Coin> GetStakeableCoins() const = 0;

  //! \brief returns the mutable proposer state for this wallet.
  virtual proposer::State &GetProposerState() = 0;

  virtual ~StakingWallet() = default;
};

}  // namespace staking

#endif  // UNIT_E_STAKINGWALLET_H
