// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_COIN_H
#define UNIT_E_STAKING_COIN_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

class CBlockIndex;

namespace staking {

//! \brief A coin that is potentially stakeable.
//!
//! A coin is basically a reference to a CTxOut of a transaction in a block.
class Coin {

 public:
  Coin(const CBlockIndex *containing_block, const COutPoint &out_point, const CTxOut &tx_out);

  //! \brief The hash of the block containing the staked coin.
  const uint256 &GetBlockHash() const;

  //! \brief The time of the block containing the staked coin.
  blockchain::Time GetBlockTime() const;

  //! \brief The amount of stake.
  CAmount GetAmount() const;

  //! \brief The height at which this coin is included in a block.
  blockchain::Height GetHeight() const;

  //! \brief The id of the transaction which spends this piece of stake.
  //!
  //! This is the same as `GetOutPoint.hash`.
  const uint256 &GetTransactionHash() const;

  //! \brief The index of the spending output.
  //!
  //! This is the same as `GetOutPoint.n`
  std::uint32_t GetOutputIndex() const;

  //! \brief The outpoint of the staking output (txid and out index).
  const COutPoint &GetOutPoint() const;

  //! \brief The locking script of the coin.
  const CScript &GetScriptPubKey() const;

  bool operator==(const Coin &other) const {
    return GetOutPoint() == other.GetOutPoint();
  }

  bool operator!=(const Coin &other) const {
    return GetOutPoint() != other.GetOutPoint();
  }

  std::string ToString() const;

 private:
  //! The index entry of the block that contains this coin.
  const CBlockIndex *containing_block;

  //! The outpoint which spends this stake.
  const COutPoint out_point;

  //! The actual CTxOut that spends this stake - featuring amount and locking script.
  CTxOut tx_out;
};

struct CoinByAmountComparator {
  bool operator()(const Coin &left, const Coin &right) const;
};

using CoinSet = std::set<Coin, CoinByAmountComparator>;

}  // namespace staking

#endif  //UNIT_E_STAKING_COIN_H
