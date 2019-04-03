// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_COINBASE_PARAMETERS_H
#define UNIT_E_COINBASE_PARAMETERS_H

#include <script/script.h>
#include <script/standard.h>
#include <settings.h>

#include <boost/optional.hpp>

namespace staking {
class StakingWallet;
}

namespace proposer {

class CoinbaseTransactionParameters {

 public:
  CoinbaseTransactionParameters() : m_reward_script(boost::none), m_stake_return_script(boost::none) {}

  CoinbaseTransactionParameters &SetRewardDestination(const CTxDestination &destination);

  CoinbaseTransactionParameters &SetRewardScript(const CScript &script);

  CoinbaseTransactionParameters &SetStakeReturnDestination(const CTxDestination &destination);

  CoinbaseTransactionParameters &SetStakeReturnScript(const CScript &script);

  CScript GetRewardScript(
      const Settings &,  //!< If no reward_script is set in this instance, take from these settings.
      const CScript &    //!< If neither this instance nor the settings define a reward address, take this script as fallback.
      ) const;

  CScript GetStakeReturnScript(
      const Settings &,          //!<
      staking::StakingWallet &,  //!<
      const CScript &            //!<
      ) const;

 private:
  //! \brief Script that the reward should be sent to.
  boost::optional<CScript> m_reward_script;

  //! \brief Script that used stake should be returned to.
  boost::optional<CScript> m_stake_return_script;
};

}  // namespace proposer

#endif  //UNIT_E_COINBASE_PARAMETERS_H
