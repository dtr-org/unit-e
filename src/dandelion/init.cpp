// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dandelion/init.h>

#include <ui_interface.h>

namespace dandelion {

const Params Params::Create(ArgsManager &args) {
  Params params;
  params.enabled = args.GetBoolArg("-dandelion", params.enabled);

  const auto embargoMin =
      args.GetArg("-dandelionmin", params.embargoMin.count());

  const auto embargoAvgAdd =
      args.GetArg("-dandelionavgadd", params.embargoAvgAdd.count());

  if (embargoMin < 0) {
    InitWarning("Negative -dandelionmin. Reverting to default");
  } else {
    params.embargoMin = std::chrono::seconds(embargoMin);
  }

  if (embargoAvgAdd < 0) {
    InitWarning("Negative -dandelionavgadd. Reverting to default");
  } else {
    params.embargoAvgAdd = std::chrono::seconds(embargoAvgAdd);
  }

  return params;
}

std::string Params::GetHelpString() {
  Params defaultParams;

  return HelpMessageOpt("-dandelion=<enable>", "Whether to use dandelion-lite: privacy enhancement protocol. True by default") +
         HelpMessageOpt("-dandelionmin=<seconds>", "Minimum dandelion embargo time. Default is " + std::to_string(defaultParams.embargoMin.count())) +
         HelpMessageOpt("-dandelionavgadd=<seconds>", "Average additive dandelion embargo time. Default is " + std::to_string(defaultParams.embargoAvgAdd.count()));
}

class SideEffectsImpl : public SideEffects {
 public:
  SideEffectsImpl(std::chrono::seconds embargoMin,
                  std::chrono::seconds embargoAvgAdd,
                  CConnman &connman)
      : m_embargoMin(embargoMin),
        m_embargoAvgAdd(embargoAvgAdd),
        m_connman(connman) {
  }

  EmbargoTime GetNextEmbargoTime() override {
    std::chrono::microseconds now = std::chrono::microseconds(GetTimeMicros());
    now += m_embargoMin;

    const auto averageIntervalSeconds =
        static_cast<int>(m_embargoAvgAdd.count());
    return PoissonNextSend(now.count(), averageIntervalSeconds);
  }

  bool IsEmbargoDue(EmbargoTime time) override {
    const auto now = GetTimeMicros();
    return time < now;
  }

  std::unordered_set<NodeId> GetOutboundNodes() override {
    std::unordered_set<NodeId> nodes;
    m_connman.ForEachNode([&nodes](CNode *node) {
      if (node->fInbound || node->fOneShot || node->fFeeler) {
        return;
      }
      const auto id = node->GetId();

      nodes.emplace(id);
    });
    return nodes;
  }

  size_t RandRange(size_t maxExcluding) override {
    return m_random.randrange(maxExcluding);
  }

  bool SendTxInv(NodeId nodeId, const uint256 &txHash) override {
    return m_connman.ForNode(nodeId, [&txHash](CNode *node) {
      // According to sdaftuar and gmaxwell
      // It is better to not send transactions directly
      // https://github.com/bitcoin/bitcoin/pull/13947/files#r210074699
      CInv inv(MSG_TX, txHash);
      node->PushInventory(inv);
      return true;
    });
  }

  void SendTxInvToAll(const uint256 &txHash) override {
    return m_connman.ForEachNode([&txHash](CNode *node) {
      // According to sdaftuar and gmaxwell
      // It is better to not send transactions directly
      // https://github.com/bitcoin/bitcoin/pull/13947/files#r210074699
      CInv inv(MSG_TX, txHash);
      node->PushInventory(inv);
    });
  }

 private:
  const std::chrono::seconds m_embargoMin;
  const std::chrono::seconds m_embargoAvgAdd;
  CConnman &m_connman;
  FastRandomContext m_random;
};

std::unique_ptr<DandelionLite> CreateDandelion(CConnman &connman,
                                               const Params &params) {
  if (!params.enabled) {
    return nullptr;
  }

  auto sideEffects = MakeUnique<SideEffectsImpl>(params.embargoMin,
                                                 params.embargoAvgAdd,
                                                 connman);

  return MakeUnique<DandelionLite>(params.timeoutsToSwitchRelay,
                                   std::move(sideEffects));
}

}  // namespace dandelion
