// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_BEHAVIOR_H
#define UNIT_E_BLOCKCHAIN_BEHAVIOR_H

#include <blockchain/blockchain_parameters.h>

#include <amount.h>
#include <blockchain/blockchain_interfaces.h>
#include <dependency.h>
#include <pubkey.h>
#include <util.h>

#include <boost/optional.hpp>

#include <cstdint>

namespace blockchain {

//! \brief Parameters as an injectable Component
//!
//! The blockchain::Parameters are supposed to be a data-only POD that is
//! not associated with any functionality. Higher layer functions like a
//! simpler way to invoke the difficulty and reward functions are placed here.
//!
//! Also this class is a proper component that fits the Injector.
class Behavior {

 private:
  const Parameters &m_parameters;

 public:
  explicit Behavior(const Parameters &) noexcept;

  //! \brief Get the timestamp usable for proposing according to Kernel protocol.
  //!
  //! \return A value less than or equal to the given timestamp.
  Time CalculateProposingTimestamp(std::int64_t timestamp_sec) const;

  //! \brief Get the NEXT timestamp for proposing.
  //!
  //! \return A value strictly greater than the given timestamp.
  Time CalculateProposingTimestampAfter(std::int64_t timestamp_sec) const;

  //! \brief Calculates the block reward given current money supply and block height.
  CAmount CalculateReward(MoneySupply, Height) const;

  //! \brief Calculates the difficulty for BlockHeight
  Difficulty CalculateDifficulty(Height, ChainAccess &) const;

  //! \brief Get a reference to the genesis block.
  //!
  //! Do not do GetGenesisBlock().Hash(), use GetGenesisBlockHash() for that,
  //! which will use a cached value and does not rehash the genesis block every
  //! time.
  const CBlock &GetGenesisBlock() const;

  //! \brief Get the hash of the genesis block, cached.
  uint256 GetGenesisBlockHash() const;

  bool IsGenesisBlock(const CBlock &) const;

  bool IsGenesisBlockHash(const uint256 &) const;

  boost::optional<CPubKey> ExtractBlockSigningKey(const CBlock &) const;

  //! \brief The name of this network as a standard string.
  std::string GetNetworkName() const;

  std::chrono::seconds GetBlockStakeTimestampInterval() const;

  const std::vector<unsigned char> &GetBase58Prefix(Base58Type) const;

  const std::string &GetBech32Prefix() const;

  const Parameters &GetParameters() const;

  static std::unique_ptr<Behavior> New(Dependency<::ArgsManager>);

  static std::unique_ptr<Behavior> NewForNetwork(Network);

  static std::unique_ptr<Behavior> NewFromParameters(const Parameters &);

  //! \brief stopgap to replace global Params() accessor function
  static void MakeGlobal(Dependency<::ArgsManager>);

  //! \brief stopgap to replace global Params() accessor function
  static Behavior &GetGlobal();

  //! \brief stopgap to set the global object from unit tests
  static void SetGlobal(std::unique_ptr<Behavior> &&);
};

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_BEHAVIOR_H
