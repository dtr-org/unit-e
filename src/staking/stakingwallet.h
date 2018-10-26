// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_STAKINGWALLET_H
#define UNIT_E_STAKING_STAKINGWALLET_H

#include <amount.h>

#include <vector>

class COutput;
class CKey;
class CWalletTx;
struct CMutableTransaction;

namespace staking {

//! \brief wallet functionality to support staking
//!
//! This class is an interface.
class StakingWallet {

 public:
  virtual CAmount GetStakeableBalance() const = 0;

  //! \brief Signs an input in a Transaction and updates that transaction.
  //!
  //! Signing an input means to add the scriptSig and witness data. These things
  //! are complected with the structure of transactions, hence a mutable
  //! transaction that will be altered by invoking this function has to be
  //! passed in.
  virtual void SignInput(
      //! The wallet that holds the private keys etc. to perform the signing.
      CWalletTx *walletTx,
      //! The transaction that hosts the input to sign
      CMutableTransaction &mutableTx,
      //! The index of the input within that transaction
      unsigned int txInIndex) const = 0;

  virtual ~StakingWallet() = default;
};

}  // namespace staking

#endif  // UNIT_E_STAKINGWALLET_H
