// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_GENESIS_H
#define UNIT_E_BLOCKCHAIN_GENESIS_H

#include <amount.h>
#include <blockchain/blockchain_parameters.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <uint256.h>

namespace blockchain {

struct P2WPKH {
  CAmount amount;
  std::string pubKeyHash;

  P2WPKH(CAmount, const std::string &&);
};

struct P2WSH {
  CAmount amount;
  std::string scriptHash;

  P2WSH(CAmount, const std::string &&);
};

struct Funds {
  Funds(std::initializer_list<P2WPKH>);
  std::vector<P2WPKH> destinations;
};

//! \brief Helper for building a genesis block.
class GenesisBlockBuilder {

 public:
  //! \brief Set the version number of the block.
  GenesisBlockBuilder &SetVersion(uint32_t);

  //! \brief Set the 32-bit unix timestamp of the block.
  GenesisBlockBuilder &SetTime(uint32_t);

  //! \brief Set the "bits" part of the block.
  GenesisBlockBuilder &SetBits(uint32_t);

  //! \brief Set the "bits" part of the block, given as difficulty.
  GenesisBlockBuilder &SetDifficulty(uint256);

  //! \brief Adds a genesis output for the public key's 160-bit hash given as hex string.
  GenesisBlockBuilder &AddFundsForPayToPubKeyHash(CAmount, const std::string &);

  //! \brief Adds a genesis output for the a P2WSH 256-bit hash given as hex string.
  GenesisBlockBuilder &AddFundsForPayToScriptHash(CAmount, const std::string &);

  //! \brief Adds a collection of funds to this block.
  GenesisBlockBuilder &Add(const Funds &&);

  //! \brief Builds the genesis block using the given parameters.
  const CBlock Build(const Parameters &) const;

 private:
  uint32_t m_version = 4;
  uint32_t m_time = 0;
  uint32_t m_bits = 0x1d00ffff;
  std::vector<std::pair<CAmount, CTxDestination>> m_initial_funds;

  const CTransactionRef BuildCoinstakeTransaction() const;
};

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_GENESIS_H
