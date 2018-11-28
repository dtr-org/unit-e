// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_rpc.h>

#include <core_io.h>

namespace proposer {

class ProposerRPCImpl : public ProposerRPC {

 private:
  Dependency<staking::ChainState> m_chain_state;
  Dependency<staking::Network> m_network;
  Dependency<MultiWallet> m_multi_wallet;
  Dependency<Proposer> m_proposer;

  UniValue GetWalletInfo(const std::vector<CWalletRef> &wallets) {
    UniValue result(UniValue::VARR);
    for (const auto &wallet : wallets) {
      const auto &walletExt = wallet->GetWalletExtension();
      const auto &proposerState = walletExt.GetProposerState();
      UniValue info(UniValue::VOBJ);
      info.pushKV("wallet", UniValue(wallet->GetName()));
      info.pushKV("balance", ValueFromAmount(wallet->GetBalance()));
      info.pushKV("stakeable_balance",
                  ValueFromAmount(walletExt.GetStakeableBalance()));
      info.pushKV("status", UniValue(proposerState.m_status._to_string()));
      info.pushKV("searches", UniValue(proposerState.m_numSearches));
      info.pushKV("searches_attempted",
                  UniValue(proposerState.m_numSearchAttempts));
      result.push_back(info);
    }
    return result;
  }

 public:
  ProposerRPCImpl(
      Dependency<staking::ChainState> chainState,
      Dependency<staking::Network> network,
      Dependency<MultiWallet> multiWallet,
      Dependency<Proposer> proposer)
      : m_chain_state(chainState),
        m_network(network),
        m_multi_wallet(multiWallet),
        m_proposer(proposer) {}

  UniValue proposerstatus(const JSONRPCRequest &request) override {
    UniValue result(UniValue::VOBJ);
    result.pushKV("wallets", GetWalletInfo(m_multi_wallet->GetWallets()));
    const auto syncStatus = m_chain_state->GetInitialBlockDownloadStatus();
    result.pushKV("sync_status", UniValue(syncStatus._to_string()));
    result.pushKV("time", DateTimeToString(GetTime()));
    const uint64_t cin = m_network->GetInboundNodeCount();
    const uint64_t cout = m_network->GetOutboundNodeCount();
    result.pushKV("incoming_connections", UniValue(cin));
    result.pushKV("outgoing_connections", UniValue(cout));
    return result;
  }

  UniValue proposerwake(const JSONRPCRequest &request) override {
    m_proposer->Wake();
    return proposerstatus(request);
  }
};

std::unique_ptr<ProposerRPC> ProposerRPC::New(
    Dependency<staking::ChainState> chainState,
    Dependency<staking::Network> network,
    Dependency<MultiWallet> multiWallet,
    Dependency<Proposer> proposer) {
  return std::unique_ptr<ProposerRPC>(
      new ProposerRPCImpl(chainState, network, multiWallet, proposer));
}

}  // namespace proposer
