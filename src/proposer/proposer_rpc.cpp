// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/proposer_rpc.h>

#include <chainparams.h>
#include <core_io.h>
#include <key_io.h>
#include <proposer/multiwallet.h>
#include <proposer/proposer.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <staking/active_chain.h>
#include <staking/network.h>
#include <wallet/wallet.h>

#include <better-enums/enum.h>
#include <tinyformat.h>

#include <algorithm>
#include <numeric>

namespace proposer {

class ProposerRPCImpl : public ProposerRPC {

 private:
  const Dependency<MultiWallet> m_multi_wallet;
  const Dependency<staking::Network> m_network;
  const Dependency<staking::ActiveChain> m_chain;
  const Dependency<Proposer> m_proposer;

  UniValue GetWalletInfo(const std::vector<std::shared_ptr<CWallet>> &wallets) const {
    UniValue result(UniValue::VARR);
    for (const auto &wallet : wallets) {
      wallet->BlockUntilSyncedToCurrentChain();
      LOCK(m_chain->GetLock());
      const auto &wallet_extension = wallet->GetWalletExtension();
      const auto &proposerState = wallet_extension.GetProposerState();
      UniValue info(UniValue::VOBJ);
      info.pushKV("wallet", UniValue(wallet->GetName()));
      {
        LOCK(wallet_extension.GetLock());
        info.pushKV("balance", ValueFromAmount(wallet->GetBalance()));
        info.pushKV("stakeable_balance", ValueFromAmount(wallet_extension.GetStakeableBalance()));
      }
      info.pushKV("status", UniValue(proposerState.m_status._to_string()));
      info.pushKV("searches", UniValue(proposerState.m_number_of_searches));
      info.pushKV("searches_attempted", UniValue(proposerState.m_number_of_search_attempts));
      info.pushKV("blocks_proposed", UniValue(proposerState.m_number_of_proposed_blocks));
      info.pushKV("transactions_included", UniValue(proposerState.m_number_of_transactions_included));
      result.push_back(info);
    }
    return result;
  }

  UniValue GetTipInfo() const {
    const CBlockIndex *const tip = m_chain->GetTip();
    if (!tip) {
      return "no tip";
    }
    const uint256 *const tip_hash = tip->phashBlock;
    assert(tip_hash);
    return ToUniValue(*tip->phashBlock);
  }

  UniValue GetGenesisInfo() const {
    const CBlockIndex *const genesis = m_chain->GetGenesis();
    if (!genesis) {
      return "no genesis";
    }
    const uint256 *const genesis_hash = genesis->phashBlock;
    assert(genesis_hash);
    return ToUniValue(*genesis->phashBlock);
  }

  UniValue GetChainInfo() const {
    LOCK(m_chain->GetLock());
    UniValue result(UniValue::VOBJ);
    result.pushKV("tip", GetTipInfo());
    result.pushKV("genesis", GetGenesisInfo());
    result.pushKV("current_height", ToUniValue(m_chain->GetHeight()));
    result.pushKV("current_size", ToUniValue(m_chain->GetSize()));
    return result;
  }

  void CheckStarted() const {
    if (!m_proposer->IsStarted()) {
      throw JSONRPCError(RPC_IN_WARMUP, "proposer is not started yet");
    }
  }

 public:
  ProposerRPCImpl(
      const Dependency<MultiWallet> multi_wallet,
      const Dependency<staking::Network> network,
      const Dependency<staking::ActiveChain> chain,
      const Dependency<Proposer> proposer)
      : m_multi_wallet(multi_wallet),
        m_network(network),
        m_chain(chain),
        m_proposer(proposer) {}

  UniValue proposerstatus(const JSONRPCRequest &request) const override {
    if (request.fHelp || request.params.size() > 0) {
      throw std::runtime_error(strprintf(
          "%s\n"
          "\n"
          "show status of the active chain and of the proposer per wallet\n",
          __func__));
    }
    UniValue result(UniValue::VOBJ);
    std::vector<std::shared_ptr<CWallet>> wallets = m_multi_wallet->GetWallets();
    result.pushKV("wallets", GetWalletInfo(wallets));
    const auto sync_status = m_chain->GetInitialBlockDownloadStatus();
    result.pushKV("sync_status", UniValue(sync_status._to_string()));
    result.pushKV("time", FormatISO8601DateTime(GetTime()));
    const uint64_t cin = m_network->GetInboundNodeCount();
    const uint64_t cout = m_network->GetOutboundNodeCount();
    result.pushKV("incoming_connections", UniValue(cin));
    result.pushKV("outgoing_connections", UniValue(cout));
    result.pushKV("active_chain", GetChainInfo());
    return result;
  }

  UniValue proposerwake(const JSONRPCRequest &request) const override {
    if (request.fHelp || request.params.size() > 0) {
      throw std::runtime_error(strprintf(
          "%s\n"
          "\n"
          "wakes the proposer and tries to propose immediately\n",
          __func__));
    }
    CheckStarted();
    m_proposer->Wake();
    return proposerstatus(request);
  }

  UniValue liststakeablecoins(const JSONRPCRequest &request) const override {
    const std::shared_ptr<CWallet> wallet = GetWalletForJSONRPCRequest(request);
    CWallet *const pwallet = wallet.get();
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
      return NullUniValue;
    }
    if (request.fHelp || request.params.size() > 0) {
      throw std::runtime_error(strprintf(
          "%s\n"
          "\n"
          "get the stakeable coins\n",
          __func__));
    }
    wallet->BlockUntilSyncedToCurrentChain();
    UniValue obj(UniValue::VOBJ);
    const staking::StakingWallet &staking_wallet = pwallet->GetWalletExtension();
    const staking::CoinSet stakeable_coins = [&]() {
      LOCK2(m_chain->GetLock(), staking_wallet.GetLock());
      return staking_wallet.GetStakeableCoins();
    }();
    CAmount stakeable_balance = 0;
    for (const auto &coin : stakeable_coins) {
      stakeable_balance += coin.GetAmount();
    }
    obj.pushKV("stakeable_balance", ValueFromAmount(stakeable_balance));
    UniValue arr(UniValue::VARR);
    for (const staking::Coin &coin : stakeable_coins) {
      arr.push_back(ToUniValue(coin));
    }
    obj.pushKV("stakeable_coins", arr);
    return obj;
  }

  UniValue ProposeBlocks(CWallet &wallet,
                         const boost::optional<CScript> &coinbase_script,
                         const int num_generate) const {

    UniValue block_hashes(UniValue::VARR);

    // To pick up to date coins for staking we need to make sure that the wallet is synced to the current chain.
    wallet.BlockUntilSyncedToCurrentChain();

    esperanza::WalletExtension &wallet_ext = wallet.GetWalletExtension();

    for (int i = 0; i < num_generate; ++i) {

      std::shared_ptr<const CBlock> block;
      {
        LOCK2(cs_main, wallet_ext.GetLock());
        const staking::CoinSet stakeable_coins = wallet_ext.GetStakeableCoins();

        if (stakeable_coins.empty()) {
          throw JSONRPCError(RPC_INTERNAL_ERROR,
                             "Not proposing, not enough balance.");
        }

        // We don't want to combine coins when we use the rpc, so we don't need
        // to pass all of them.
        const staking::CoinSet first_coin = {*stakeable_coins.begin()};

        block = m_proposer->GenerateBlock(wallet,
                                          first_coin,
                                          coinbase_script);
        if (!block) {
          throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to generate a block.");
        }
      }

      if (!ProcessNewBlock(Params(), block, /* fForceProcessing= */ true, nullptr)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
      }

      block_hashes.push_back(block->GetHash().GetHex());

      wallet.BlockUntilSyncedToCurrentChain();
    }
    return block_hashes;
  }

  UniValue propose(const JSONRPCRequest &request) const override {
    CWallet *const pwallet = GetWalletForJSONRPCRequest(request).get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
      return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
      throw std::runtime_error(
          "propose nblocks\n"
          "\nPropose up to nblocks blocks immediately (before the RPC call returns) to an address in the wallet.\n"
          "\nNote: this function can only be used on the regtest network.\n"
          "\nArguments:\n"
          "1. nblocks      (numeric, required) How many blocks are proposed immediately.\n"
          "\nResult:\n"
          "[ blockhashes ]     (array) hashes of blocks proposed\n"
          "\nExamples:\n"
          "\nGenerate 11 blocks\n" +
          HelpExampleCli("propose", "11"));
    }

    if (!Params().MineBlocksOnDemand()) {
      throw JSONRPCError(RPC_METHOD_NOT_FOUND, "This method can only be used on regtest");
    }

    int num_generate = request.params[0].get_int();

    return ProposeBlocks(*pwallet, boost::none, num_generate);
  }

  UniValue proposetoaddress(const JSONRPCRequest &request) const override {
    CWallet *const pwallet = GetWalletForJSONRPCRequest(request).get();

    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
      return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
      throw std::runtime_error(
          "proposetoaddress nblocks address\n"
          "\nProposes up to nBlocks immediately to a specified address (before the RPC call returns)\n"
          "\nNote: this function can only be used on the regtest network.\n"
          "\nArguments:\n"
          "1. nblocks      (numeric, required) How many blocks are proposed immediately.\n"
          "2. address      (string, required) The address to send the newly proposed unite to.\n"
          "\nResult:\n"
          "[ blockhashes ]     (array) hashes of blocks generated\n"
          "\nExamples:\n"
          "\nGenerate 11 blocks to myaddress\n" +
          HelpExampleCli("proposetoaddress", "11 \"myaddress\""));

    if (!Params().MineBlocksOnDemand()) {
      throw JSONRPCError(RPC_METHOD_NOT_FOUND, "This method can only be used on regtest");
    }

    int nGenerate = request.params[0].get_int();

    CTxDestination destination = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(destination)) {
      throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Error: Invalid address");
    }

    return ProposeBlocks(*pwallet,
                         GetScriptForDestination(destination),
                         nGenerate);
  }
};

std::unique_ptr<ProposerRPC> ProposerRPC::New(
    const Dependency<MultiWallet> multi_wallet,
    const Dependency<staking::Network> network,
    const Dependency<staking::ActiveChain> chain,
    const Dependency<Proposer> proposer) {
  return std::unique_ptr<ProposerRPC>(
      new ProposerRPCImpl(multi_wallet, network, chain, proposer));
}

}  // namespace proposer
