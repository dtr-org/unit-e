// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_FINALIZER_COMMITS_HANDLER_IMPL
#define UNITE_P2P_FINALIZER_COMMITS_HANDLER_IMPL

#include <p2p/finalizer_commits_handler.h>

#include <chain.h>

namespace esperanza {
class FinalizationState;
}

namespace finalization {
using FinalizationState = esperanza::FinalizationState;
}

namespace p2p {

class FinalizerCommitsHandlerImpl : public ::p2p::FinalizerCommitsHandler {
 public:
  FinalizerCommitsHandlerImpl(Dependency<staking::ActiveChain> active_chain,
                              Dependency<finalization::StateRepository> repo,
                              Dependency<finalization::StateProcessor> proc)
      : m_active_chain(active_chain),
        m_repo(repo),
        m_proc(proc) {}

  void OnGetCommits(
      CNode &node, const FinalizerCommitsLocator &locator, const Consensus::Params &params) const override;

  bool OnCommits(
      CNode &node,
      const FinalizerCommitsResponse &msg,
      const CChainParams &chainparams,
      CValidationState &err_state,
      uint256 *failed_block_out) override;

 protected:
  const CBlockIndex *FindMostRecentStart(const FinalizerCommitsLocator &locator) const;

  const CBlockIndex *FindStop(const FinalizerCommitsLocator &locator) const;

  HeaderAndFinalizerCommits FindHeaderAndFinalizerCommits(
      const CBlockIndex &index, const Consensus::Params &params) const;

 private:
  Dependency<staking::ActiveChain> m_active_chain;
  Dependency<finalization::StateRepository> m_repo;
  Dependency<finalization::StateProcessor> m_proc;
};

}  // namespace p2p

#endif
