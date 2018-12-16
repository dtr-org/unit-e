// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_LOGIC_H
#define UNIT_E_PROPOSER_LOGIC_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <dependency.h>
#include <staking/active_chain.h>
#include <staking/network.h>
#include <staking/stake_validator.h>

#include <boost/optional.hpp>

#include <memory>

class COutput;

namespace proposer {

class Logic {
 public:
  //! \brief
  //!
  //!
  virtual boost::optional<COutput> TryPropose(const std::vector<COutput> &) = 0;

  virtual ~Logic() = default;

  static std::unique_ptr<Logic> New(
      Dependency<blockchain::Behavior>,
      Dependency<staking::ActiveChain>,
      Dependency<staking::Network>,
      Dependency<staking::StakeValidator>);
};

}  // namespace proposer

#endif  //UNIT_E_PROPOSER_LOGIC_H
