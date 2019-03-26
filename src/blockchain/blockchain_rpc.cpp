// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_rpc.h>

#include <rpc/util.h>

namespace blockchain {

class BlockchainRPCImpl : public BlockchainRPC {

 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;

 public:
  BlockchainRPCImpl(Dependency<blockchain::Behavior> blockchain_behavior)
      : m_blockchain_behavior(blockchain_behavior) {}

  UniValue getchainparams(const JSONRPCRequest &request) const override {
    UniValue parameters(UniValue::VOBJ);

#define PUT_PARAMETER(NAME) \
  parameters.pushKV(#NAME, ToUniValue(m_blockchain_behavior->GetParameters().NAME));

    PUT_PARAMETER(network_name);
    PUT_PARAMETER(block_stake_timestamp_interval_seconds);
    PUT_PARAMETER(block_time_seconds);
    PUT_PARAMETER(max_future_block_time_seconds);
    PUT_PARAMETER(relay_non_standard_transactions);
    PUT_PARAMETER(maximum_block_size);
    PUT_PARAMETER(maximum_block_weight);
    PUT_PARAMETER(maximum_block_serialized_size);
    PUT_PARAMETER(maximum_block_sigops_cost);
    PUT_PARAMETER(coinbase_maturity);
    PUT_PARAMETER(stake_maturity);
    PUT_PARAMETER(initial_supply);
    PUT_PARAMETER(maximum_supply);
    PUT_PARAMETER(reward_schedule);
    PUT_PARAMETER(period_blocks);
    PUT_PARAMETER(mine_blocks_on_demand);
    PUT_PARAMETER(base58_prefixes);
    PUT_PARAMETER(bech32_human_readable_prefix);
    PUT_PARAMETER(deployment_confirmation_period);
    PUT_PARAMETER(rule_change_activation_threshold);
    PUT_PARAMETER(genesis_block);

#undef PUT_PARAMETER

    return parameters;
  };
};

std::unique_ptr<BlockchainRPC> BlockchainRPC::New(Dependency<blockchain::Behavior> blockchain_behavior) {
  return std::unique_ptr<BlockchainRPC>(new BlockchainRPCImpl(blockchain_behavior));
}

}  // namespace blockchain
