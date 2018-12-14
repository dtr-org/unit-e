// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_rpc.h>

#include <core_io.h>
#include <wallet/wallet.h>

namespace proposer {

class ProposerRPCImpl : public ProposerRPC {

 private:
  Dependency<MultiWallet> m_multi_wallet;
  Dependency<staking::Network> m_network;
  Dependency<staking::ActiveChain> m_chain;
  Dependency<Proposer> m_proposer;

  UniValue GetWalletInfo(const std::vector<CWalletRef> &wallets) {
    LOCK(m_chain->GetLock());
    UniValue result(UniValue::VARR);
    for (const auto &wallet : wallets) {
      const auto &wallet_extension = wallet->GetWalletExtension();
      const auto &proposerState = wallet_extension.GetProposerState();
      UniValue info(UniValue::VOBJ);
      info.pushKV("wallet", UniValue(wallet->GetName()));
      {
        LOCK(wallet_extension.GetLock());
        info.pushKV("balance", ValueFromAmount(wallet->GetBalance()));
        info.pushKV("stakeable_balance",
                    ValueFromAmount(wallet_extension.GetStakeableBalance()));
      }
      info.pushKV("status", UniValue(proposerState.m_status._to_string()));
      info.pushKV("searches", UniValue(proposerState.m_number_of_searches));
      info.pushKV("searches_attempted",
                  UniValue(proposerState.m_number_of_search_attempts));
      result.push_back(info);
    }
    return result;
  }

 public:
  ProposerRPCImpl(
      Dependency<MultiWallet> multi_wallet,
      Dependency<staking::Network> network,
      Dependency<staking::ActiveChain> chain,
      Dependency<Proposer> proposer)
      : m_multi_wallet(multi_wallet),
        m_network(network),
        m_chain(chain),
        m_proposer(proposer) {}

  UniValue proposerstatus(const JSONRPCRequest &request) override {
    UniValue result(UniValue::VOBJ);
    result.pushKV("wallets", GetWalletInfo(m_multi_wallet->GetWallets()));
    const auto sync_status = m_chain->GetInitialBlockDownloadStatus();
    result.pushKV("sync_status", UniValue(sync_status._to_string()));
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
    Dependency<MultiWallet> multi_wallet,
    Dependency<staking::Network> network,
    Dependency<staking::ActiveChain> chain,
    Dependency<Proposer> proposer) {
  return std::unique_ptr<ProposerRPC>(
      new ProposerRPCImpl(multi_wallet, network, chain, proposer));
}

}  // namespace proposer
