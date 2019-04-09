// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_FINALIZER_COMMITS_HANDLER_IMPL_H
#define UNITE_P2P_FINALIZER_COMMITS_HANDLER_IMPL_H

#include <p2p/finalizer_commits_handler.h>

#include <chain.h>

namespace esperanza {
class FinalizationState;
}

namespace finalization {
using FinalizationState = esperanza::FinalizationState;
}

namespace p2p {

class FinalizerCommitsHandlerImpl : public FinalizerCommitsHandler {
 public:
  FinalizerCommitsHandlerImpl(Dependency<staking::ActiveChain> active_chain,
                              Dependency<finalization::StateRepository> repo,
                              Dependency<finalization::StateProcessor> proc)
      : m_active_chain(active_chain),
        m_repo(repo),
        m_proc(proc) {}

  FinalizerCommitsLocator GetFinalizerCommitsLocator(
      const CBlockIndex &start, const CBlockIndex *stop) const override;

  void OnGetCommits(
      CNode &node, const FinalizerCommitsLocator &locator, const Consensus::Params &params) const override;

  bool OnCommits(
      CNode &node,
      const FinalizerCommitsResponse &msg,
      const CChainParams &chainparams,
      CValidationState &err_state,
      uint256 *failed_block_out) override;

  void OnDisconnect(NodeId nodeid) override;

  bool FindNextBlocksToDownload(
      NodeId nodeid, size_t count, std::vector<const CBlockIndex *> &blocks_out) override;

  const CBlockIndex *GetLastFinalizedCheckpoint() const override;

 protected:
  const CBlockIndex *FindMostRecentStart(const FinalizerCommitsLocator &locator) const;

  const CBlockIndex *FindStop(const FinalizerCommitsLocator &locator) const;

  //! \brief Returns whether test is an ancestor of the head.
  //!
  //! Pseudo code:
  //! if (test->pprev == prev) return true;
  //! else return head->GetAncestor(test->nHeight) == test;
  //!
  //! Saves the ancestor in the prev so that this function is optimized for a case of serial
  //! invocations on `test` indexes with continuously growing height.
  //!
  //! Example:
  //!
  //! // p0 -> p1 -> p2 -> head
  //! CBlockIndex prev;
  //! IsSameFork(head, p0, prev); // not optimized, saves p0 in prev
  //! IsSameFork(head, p1, prev); // optimized (p1->prev == pprev). saves p1 in prev
  //! IsSameFork(head, p2, prev); // optimized (p2->prev == pprev). saves p2 in prev
  //!
  static bool IsSameFork(const CBlockIndex *head, const CBlockIndex *test, const CBlockIndex *&prev);

 private:
  const CBlockIndex &FindLastFinalizedCheckpoint(
      const finalization::FinalizationState &fin_state) const;

  const CBlockIndex &GetCheckpointIndex(
      uint32_t epoch, const finalization::FinalizationState &fin_state) const;

  boost::optional<HeaderAndFinalizerCommits> FindHeaderAndFinalizerCommits(
      const CBlockIndex &index, const Consensus::Params &params) const;

  Dependency<staking::ActiveChain> m_active_chain;
  Dependency<finalization::StateRepository> m_repo;
  Dependency<finalization::StateProcessor> m_proc;

  struct HeightComparator {
    inline bool operator()(const CBlockIndex *a, const CBlockIndex *b) const {
      return a->nHeight < b->nHeight;
    }
  };

  mutable CCriticalSection cs;
  std::map<NodeId, std::multiset<const CBlockIndex *, HeightComparator>> m_wait_list;
  std::map<NodeId, std::list<const CBlockIndex *>> m_blocks_to_download;
  //! The last finalized checkpoint.
  //!  F  J votes
  //! e1 e2 e3
  //! It's a checkpoint of epoch e1.
  const CBlockIndex *m_last_finalized_checkpoint = nullptr;
  //! The point in the chain where finalization happened.
  //!  F  J votes
  //! e1 e2 e3
  //! It's one of the index from epoch e3.
  const CBlockIndex *m_last_finalization_point = nullptr;
};

}  // namespace p2p

#endif
