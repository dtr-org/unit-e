// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/embargoman_init.h>

namespace p2p {

bool EmbargoManParams::Create(const ArgsManager &args,
                              p2p::EmbargoManParams &params_out,
                              std::string &error_message_out) {
  EmbargoManParams params;
  params.enabled = args.GetBoolArg("-embargotxs", params.enabled);

  const auto embargo_min =
      args.GetArg("-embargomin", params.embargo_min.count());

  const auto embargo_avg_add =
      args.GetArg("-embargoavgadd", params.embargo_avg_add.count());

  if (embargo_min < 0) {
    error_message_out = "Negative -embargomin";
    return false;
  }

  if (embargo_avg_add < 0) {
    error_message_out = "Negative -embargoavgadd";
    return false;
  }

  params.embargo_min = std::chrono::seconds(embargo_min);
  params.embargo_avg_add = std::chrono::seconds(embargo_avg_add);

  params_out = params;
  return true;
}

class SideEffectsImpl : public EmbargoManSideEffects {
 public:
  SideEffectsImpl(std::chrono::seconds embargo_min,
                  std::chrono::seconds embargo_avg_add,
                  CConnman &connman)
      : m_embargo_min(embargo_min),
        m_embargo_avg_add(embargo_avg_add),
        m_connman(connman) {
  }

  EmbargoTime GetNextEmbargoTime() override {
    std::chrono::microseconds now = std::chrono::microseconds(GetTimeMicros());
    now += m_embargo_min;

    const auto average_interval_seconds =
        static_cast<int>(m_embargo_avg_add.count());
    return PoissonNextSend(now.count(), average_interval_seconds);
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

  size_t RandRange(size_t max_excluding) override {
    return m_random.randrange(max_excluding);
  }

  bool SendTxInv(NodeId node_id, const uint256 &tx_hash) override {
    return m_connman.ForNode(node_id, [&tx_hash](CNode *node) {
      // According to sdaftuar and gmaxwell
      // It is better to not send transactions directly
      // https://github.com/unite/unite/pull/13947/files#r210074699
      CInv inv(MSG_TX, tx_hash);
      node->PushInventory(inv);
      return true;
    });
  }

  void SendTxInvToAll(const uint256 &tx_hash) override {
    return m_connman.ForEachNode([&tx_hash](CNode *node) {
      // According to sdaftuar and gmaxwell
      // It is better to not send transactions directly
      // https://github.com/unite/unite/pull/13947/files#r210074699
      CInv inv(MSG_TX, tx_hash);
      node->PushInventory(inv);
    });
  }

 private:
  const std::chrono::seconds m_embargo_min;
  const std::chrono::seconds m_embargo_avg_add;
  CConnman &m_connman;
  FastRandomContext m_random;
};

std::unique_ptr<EmbargoMan> CreateEmbargoMan(CConnman &connman,
                                             const EmbargoManParams &params) {
  if (!params.enabled) {
    return nullptr;
  }

  auto side_effects = MakeUnique<SideEffectsImpl>(params.embargo_min,
                                                  params.embargo_avg_add,
                                                  connman);

  return MakeUnique<EmbargoMan>(params.timeouts_to_switch_relay,
                                std::move(side_effects));
}

}  // namespace p2p
