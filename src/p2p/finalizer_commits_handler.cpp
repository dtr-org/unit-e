// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/finalizer_commits_handler.h>

#include <p2p/finalizer_commits_handler_impl.h>
#include <util.h>

namespace p2p {

std::unique_ptr<FinalizerCommitsHandler> FinalizerCommitsHandler::New(
    Dependency<staking::ActiveChain> active_chain,
    Dependency<finalization::StateRepository> state_repo,
    Dependency<finalization::StateProcessor> state_proc) {

  return MakeUnique<FinalizerCommitsHandlerImpl>(active_chain, state_repo, state_proc);
}

}  // namespace p2p
