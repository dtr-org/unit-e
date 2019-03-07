// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_FINALIZER_COMMITS
#define UNITE_P2P_FINALIZER_COMMITS

#include <consensus/params.h>
#include <dependency.h>
#include <net.h>
#include <p2p/finalizer_commits_types.h>

#include <memory>

class CBlockIndex;
class CChainParams;
class CNode;
class CValidationState;

namespace finalization {
class StateRepository;
class StateProcessor;
}  // namespace finalization

namespace staking {
class ActiveChain;
}  // namespace staking

namespace p2p {

class FinalizerCommits {
 public:
  //! \brief Process getcommits message
  virtual void OnGetCommits(
      CNode &from, const FinalizerCommitsLocator &locator, const Consensus::Params &params) const = 0;

  //! \brief Process commits message
  virtual bool OnCommits(
      CNode &node,
      const FinalizerCommitsResponse &msg,
      const CChainParams &chainparams,
      CValidationState &err_state,
      uint256 *failed_block_out) = 0;

  virtual ~FinalizerCommits() = default;

  static std::unique_ptr<FinalizerCommits> New(
      Dependency<staking::ActiveChain>,
      Dependency<finalization::StateRepository>,
      Dependency<finalization::StateProcessor>);
};

}  // namespace p2p

#endif
