// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_PROPOSER_PROPOSER_H
#define UNITE_PROPOSER_PROPOSER_H

#include <dependency.h>
#include <primitives/block.h>
#include <proposer/block_builder.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer_logic.h>
#include <proposer/proposer_status.h>
#include <proposer/sync.h>
#include <proposer/waiter.h>
#include <settings.h>
#include <staking/active_chain.h>
#include <staking/network.h>
#include <staking/transactionpicker.h>

#include <map>
#include <memory>

class CWallet;

namespace proposer {

class Proposer {

 public:
  virtual void Wake() = 0;

  virtual void Start() = 0;

  virtual void Stop() = 0;

  virtual ~Proposer() = default;

  static std::unique_ptr<Proposer> New(Dependency<Settings>,
                                       Dependency<blockchain::Behavior>,
                                       Dependency<MultiWallet>,
                                       Dependency<staking::Network>,
                                       Dependency<staking::ActiveChain>,
                                       Dependency<staking::TransactionPicker>,
                                       Dependency<proposer::BlockBuilder>,
                                       Dependency<proposer::Logic>);
};

}  // namespace proposer

#endif  // UNITE_PROPOSER_PROPOSER_H
