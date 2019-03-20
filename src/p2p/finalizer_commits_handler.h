// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_FINALIZER_COMMITS_HANDLER_H
#define UNITE_P2P_FINALIZER_COMMITS_HANDLER_H

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

//! \brief Handler of finalizer commits-related p2p messages.
class FinalizerCommitsHandler {
 public:
  //! \brief Returns a CommitsLocator
  //!
  //! locator.start = [finalized-checkpoint,.. checkpoints,.. start]
  //! locator.stop = stop
  virtual FinalizerCommitsLocator GetFinalizerCommitsLocator(
      const CBlockIndex &start, const CBlockIndex *stop) const = 0;

  //! \brief Process getcommits message
  virtual void OnGetCommits(
      CNode &node, const FinalizerCommitsLocator &locator, const Consensus::Params &params) const = 0;

  //! \brief Process commits message
  virtual bool OnCommits(
      CNode &node,
      const FinalizerCommitsResponse &msg,
      const CChainParams &chainparams,
      CValidationState &err_state,
      uint256 *failed_block_out) = 0;

  //! \brief Process peer disconnection
  virtual void OnDisconnect(NodeId nodeid) = 0;

  //! \brief Find whether we need to download blocks to satisfy commits full sync
  //
  // Returns true when blocks_out modified.
  virtual bool FindNextBlocksToDownload(
      NodeId nodeid, size_t count, std::vector<const CBlockIndex *> &blocks_out) = 0;

  virtual ~FinalizerCommitsHandler() = default;

  static std::unique_ptr<FinalizerCommitsHandler> New(
      Dependency<staking::ActiveChain>,
      Dependency<finalization::StateRepository>,
      Dependency<finalization::StateProcessor>);
};

}  // namespace p2p

#endif
