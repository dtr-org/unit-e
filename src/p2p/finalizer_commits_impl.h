// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/finalizer_commits.h>

#include <chain.h>

namespace esperanza {
class FinalizationState;
}

namespace finalization {
using FinalizationState = esperanza::FinalizationState;
}

namespace p2p {
namespace impl {

class FinalizerCommits : public ::p2p::FinalizerCommits {
 public:
  FinalizerCommits(Dependency<staking::ActiveChain> active_chain,
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

  //! \brief Returns wheter test is an ancestor of the head.
  //!
  //! Optimized for a case of serial invocations on `test` indexes with continuously growing height.
  static bool IsSameFork(const CBlockIndex *head, const CBlockIndex *test, const CBlockIndex *&prev);

 private:
  Dependency<staking::ActiveChain> m_active_chain;
  Dependency<finalization::StateRepository> m_repo;
  Dependency<finalization::StateProcessor> m_proc;

  struct HeightComparator {
    bool operator()(const CBlockIndex *a, const CBlockIndex *b) const {
      return a->nHeight < b->nHeight;
    }
  };

  std::map<NodeId, std::multiset<const CBlockIndex *, HeightComparator>> m_wait_list;
  std::map<NodeId, std::list<const CBlockIndex *>> m_to_download;
};

}  // namespace impl
}  // namespace p2p
