// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_BEHAVIOR_H
#define UNIT_E_BLOCKCHAIN_BEHAVIOR_H

#include <blockchain/blockchain_parameters.h>

#include <amount.h>
#include <blockchain/blockchain_interfaces.h>
#include <dependency.h>
#include <util.h>

#include <cstdint>

namespace blockchain {

//! \brief Simpler access to Parameters
//!
//! The parameters class is designed to purely hold data, to easily be read,
//! understood, and edited. It is a bit less comfortable when it actually using
//! it. That is encapsulated to this "Behavior" class (for the lack of a better
//! name).
class Behavior {

 private:
  const Parameters &m_parameters;

 public:
  Behavior(const Parameters &);

  //! \brief Get the timestamp usable for proposing according to Kernel protocol.
  //!
  //! \return A value less than or equal to the given timestamp.
  std::uint32_t CalculateProposingTimestamp(std::uint64_t timestamp_sec) const;

  //! \brief Get the NEXT timestamp for proposing.
  //!
  //! \return A value strictly greater than the given timestamp.
  std::uint32_t CalculateProposingTimestampAfter(std::uint64_t timestamp_sec) const;

  //! \brief Calculates the block reward given current money supply and block height.
  CAmount CalculateReward(MoneySupply, Height) const;

  //! \brief Calculates the difficulty for BlockHeight
  Difficulty CalculateDifficulty(Height, ChainAccess&) const;

  //! \brief Get a reference to the genesis block.
  //!
  //! Do not do GetGenesisBlock().Hash(), use GetGenesisBlockHash() for that,
  //! which will use a cached value and does not rehash the genesis block every
  //! time.
  const CBlock &GetGenesisBlock() const;

  //! \brief Get the hash of the genesis block, cached.
  uint256 GetGenesisBlockHash() const;

  //! \brief The name of this network as a standard string.
  std::string GetNetworkName() const;

  static std::unique_ptr<Behavior> New(Dependency<::ArgsManager>);
};

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_BEHAVIOR_H
