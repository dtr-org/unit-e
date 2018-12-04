// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/embargoman_init.h>

namespace p2p {

bool EmbargoManParams::Create(ArgsManager &args,
                              p2p::EmbargoManParams &paramsOut,
                              std::string &errorMessageOut) {
  EmbargoManParams params;
  params.enabled = args.GetBoolArg("-embargotxs", params.enabled);

  const auto embargoMin =
      args.GetArg("-embargomin", params.embargoMin.count());

  const auto embargoAvgAdd =
      args.GetArg("-embargoavgadd", params.embargoAvgAdd.count());

  if (embargoMin < 0) {
    errorMessageOut = "Negative -embargomin";
    return false;
  }

  if (embargoAvgAdd < 0) {
    errorMessageOut = "Negative -embargoavgadd";
    return false;
  }

  params.embargoMin = std::chrono::seconds(embargoMin);
  params.embargoAvgAdd = std::chrono::seconds(embargoAvgAdd);

  paramsOut = params;
  return true;
}

std::string EmbargoManParams::GetHelpString() {
  EmbargoManParams defaultParams;

  return HelpMessageOpt("-embargotxs=<enable>", "Whether to use embargoing mechanism(aka Dandelion Lite). True by default") +
         HelpMessageOpt("-embargomin=<seconds>", "Minimum embargo time. Default is " + std::to_string(defaultParams.embargoMin.count())) +
         HelpMessageOpt("-embargoavgadd=<seconds>", "Average additive embargo time. Default is " + std::to_string(defaultParams.embargoAvgAdd.count()));
}

class SideEffectsImpl : public EmbargoManSideEffects {
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
    const EmbargoTime now = GetTimeMicros();
    return time < now;
  }

  std::set<NodeId> GetOutboundNodes() override {
    std::set<NodeId> nodes;
    m_connman.ForEachNode([&nodes](CNode *node) {
      if (node->fInbound || node->fOneShot || node->fFeeler) {
        return;
      }

      nodes.emplace(node->GetId());
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

std::unique_ptr<EmbargoMan> CreateEmbargoMan(CConnman &connman,
                                             const EmbargoManParams &params) {
  if (!params.enabled) {
    return nullptr;
  }

  auto sideEffects = MakeUnique<SideEffectsImpl>(params.embargoMin,
                                                 params.embargoAvgAdd,
                                                 connman);

  return MakeUnique<EmbargoMan>(params.timeoutsToSwitchRelay,
                                std::move(sideEffects));
}

}  // namespace network
