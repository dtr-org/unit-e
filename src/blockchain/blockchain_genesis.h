// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_GENESIS_H
#define UNIT_E_BLOCKCHAIN_GENESIS_H

#include <amount.h>
#include <primitives/block.h>
#include <uint256.h>

namespace blockchain {

struct GenesisOutput {
  CAmount amount;
  std::string pubKeyHash;

  GenesisOutput(CAmount, const std::string &&);
};

struct GenesisBlock {
  CBlock block;
  uint256 hash;

  GenesisBlock() = default;
  GenesisBlock(std::initializer_list<GenesisOutput>);
};

//! \brief Helper for building a genesis block.
class GenesisBlockBuilder {

 public:
  //! \brief Set the version number of the block.
  virtual void SetVersion(uint32_t) = 0;

  //! \brief Set the 32-bit unix timestamp of the block.
  virtual void SetTime(uint32_t) = 0;

  //! \brief Set the "bits" part of the block.
  virtual void SetBits(uint32_t) = 0;

  //! \brief Set the "bits" part of the block, given as difficulty.
  virtual void SetDifficulty(uint256) = 0;

  //! \brief Adds a genesis output for the key given as a hex string.
  virtual void AddFundsForPubKey(CAmount, const std::string &) = 0;

  //! \brief Adds a genesis output for the public key's 160-bit hash given as hex string.
  virtual void AddFundsForPubKeyHash(CAmount, const std::string &) = 0;

  //! \brief Builds the genesis block using the given parameters.
  virtual const GenesisBlock Build() const = 0;

  static std::unique_ptr<GenesisBlockBuilder> New();

  virtual ~GenesisBlockBuilder() = default;
};

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_GENESIS_H
