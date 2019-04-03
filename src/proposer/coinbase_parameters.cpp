// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/coinbase_parameters.h>
#include <staking/stakingwallet.h>

#include <boost/variant.hpp>

namespace proposer {

CoinbaseTransactionParameters &CoinbaseTransactionParameters::SetRewardDestination(const CTxDestination &destination) {
  m_reward_script = GetScriptForDestination(destination);
  return *this;
}

CoinbaseTransactionParameters &CoinbaseTransactionParameters::SetRewardScript(const CScript &script) {
  m_reward_script = script;
  return *this;
}

CoinbaseTransactionParameters &CoinbaseTransactionParameters::SetStakeReturnDestination(const CTxDestination &destination) {
  m_stake_return_script = GetScriptForDestination(destination);
  return *this;
}

CoinbaseTransactionParameters &CoinbaseTransactionParameters::SetStakeReturnScript(const CScript &script) {
  m_stake_return_script = script;
  return *this;
}

CScript CoinbaseTransactionParameters::GetRewardScript(const Settings &settings, const CScript &fallback_script) const {
  if (m_reward_script) {
    return *m_reward_script;
  }
  if (settings.reward_destination) {
    return GetScriptForDestination(*settings.reward_destination);
  }
  return fallback_script;
}

class StakeReturnScriptVisitor : public boost::static_visitor<CScript> {
 private:
  staking::StakingWallet &m_wallet;
  const CScript &m_fallback_script;

 public:
  StakeReturnScriptVisitor(staking::StakingWallet &wallet, const CScript &fallback_script)
      : m_wallet(wallet), m_fallback_script(fallback_script) {}

  CScript operator()(staking::ReturnStakeToSameAddress) const {
    return m_fallback_script;
  }

  CScript operator()(staking::ReturnStakeToNewAddress) const {
    return m_wallet.GetScriptForStaking();
  }

  CScript operator()(const CScript &target_script) const {
    return target_script;
  }
};

CScript CoinbaseTransactionParameters::GetStakeReturnScript(
    const Settings &settings, staking::StakingWallet &wallet, const CScript &fallback_script) const {
  StakeReturnScriptVisitor visitor(wallet, fallback_script);
  return boost::apply_visitor(visitor, settings.stake_return_mode);
}

}  // namespace proposer
