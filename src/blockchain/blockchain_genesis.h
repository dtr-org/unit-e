// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_BLOCKCHAIN_BLOCKCHAIN_GENESIS_H
#define UNITE_BLOCKCHAIN_BLOCKCHAIN_GENESIS_H

#include <amount.h>
#include <blockchain/blockchain_parameters.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <uint256.h>

namespace blockchain {

struct P2WPKH {
  CAmount amount;
  std::string pub_key_hash;

  P2WPKH() = default;
  P2WPKH(CAmount, std::string &&);
};

struct P2WSH {
  CAmount amount;
  std::string script_hash;

  P2WSH() = default;
  P2WSH(CAmount, std::string &&);
};

struct Funds {
  Funds(std::initializer_list<P2WPKH>);
  std::vector<P2WPKH> destinations;
};

Funds TestnetFunds();
Funds RegtestFunds();

//! \brief Helper for building a genesis block.
class GenesisBlockBuilder {

 public:
  //! \brief Set the version number of the block.
  GenesisBlockBuilder &SetVersion(int32_t);

  //! \brief Set the 32-bit unix timestamp of the block.
  GenesisBlockBuilder &SetTime(blockchain::Time);

  //! \brief Set the "bits" part of the block.
  GenesisBlockBuilder &SetBits(blockchain::Difficulty);

  //! \brief Set the "bits" part of the block, given as difficulty.
  GenesisBlockBuilder &SetDifficulty(uint256);

  //! \brief Adds a genesis output for the public key's 160-bit hash given as hex string.
  GenesisBlockBuilder &AddFundsForPayToPubKeyHash(CAmount, const std::string &);

  //! \brief Adds a genesis output for the a P2WSH 256-bit hash given as hex string.
  GenesisBlockBuilder &AddFundsForPayToScriptHash(CAmount, const std::string &);

  //! \brief Adds a collection of funds to this block.
  GenesisBlockBuilder &Add(Funds &&);

  //! \brief Builds the genesis block using the given parameters.
  const CBlock Build(const Parameters &) const;

 private:
  std::int32_t m_version = 4;
  blockchain::Time m_time = 0;
  blockchain::Difficulty m_bits = 0x1d00ffff;
  std::vector<std::pair<CAmount, CTxDestination>> m_initial_funds;

  const CTransactionRef BuildCoinbaseTransaction() const;
};

}  // namespace blockchain

#endif  // UNITE_BLOCKCHAIN_BLOCKCHAIN_GENESIS_H
