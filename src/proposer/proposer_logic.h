// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_LOGIC_H
#define UNIT_E_PROPOSER_LOGIC_H

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <dependency.h>
#include <proposer/eligible_coin.h>
#include <staking/active_chain.h>
#include <staking/network.h>
#include <staking/stake_validator.h>

#include <boost/optional.hpp>

#include <memory>

class COutput;

namespace proposer {

class Logic {
 public:
  //! \brief Given a list of stakeable coins, checks which can be used for proposing (if any).
  //!
  //! Being eligible for proposing requires to "win the lottery", which is finding
  //! a stakeable coin which meets the proof-of-stake requirements. This function
  //! finds such a coin from a list of stakeable coins, if there is any.
  //!
  //! The actual proposer component can then proceed and assemble a block and
  //! broadcast it into the network.
  virtual boost::optional<proposer::EligibleCoin> TryPropose(const std::vector<COutput> &) = 0;

  virtual ~Logic() = default;

  static std::unique_ptr<Logic> New(
      Dependency<blockchain::Behavior>,
      Dependency<staking::Network>,
      Dependency<staking::ActiveChain>,
      Dependency<staking::StakeValidator>);
};

}  // namespace proposer

#endif  //UNIT_E_PROPOSER_LOGIC_H
