// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/staking_rpc.h>

#include <blockdb.h>
#include <chain.h>
#include <core_io.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rpc/util.h>
#include <staking/active_chain.h>

#include <better-enums/enum.h>
#include <tinyformat.h>
#include <boost/variant.hpp>

#include <algorithm>
#include <numeric>

namespace staking {

static constexpr const std::size_t default_length = 100;
static constexpr const bool default_reverse = false;

class StakingRPCImpl : public StakingRPC {

 private:
  const Dependency<staking::ActiveChain> m_chain;
  const Dependency<::BlockDB> m_block_db;

  struct GenesisBlock {
    const CBlockIndex *block;
  };
  struct NotOnDisk {
    const CBlockIndex *block;
  };
  struct NoStakeFound {
    const CBlockIndex *block;
    CTxIn stake_in;
  };
  struct InvalidBlock {
    const CBlockIndex *block;
    const char *reason;
  };
  struct InvalidReference {
    const CBlockIndex *block;
    CTransactionRef tx;
    CTxIn stake_in;
  };
  struct StakeInfo {
    const CBlockIndex *funding_block;
    const CBlockIndex *spending_block;
    CTxOut stake_out;
    CTxIn stake_in;
  };

  using StakeMeta = boost::variant<GenesisBlock, NotOnDisk, NoStakeFound, InvalidBlock, InvalidReference, StakeInfo>;

  static inline void BlockInfo(UniValue &obj, const CBlockIndex &block) {
    obj.pushKV("block_hash", ToUniValue(*block.phashBlock));
    obj.pushKV("block_height", ToUniValue(block.nHeight));
  }
  static inline void TransactionInfo(UniValue &obj, const CTransactionRef &tx) {
    obj.pushKV("txid", ToUniValue(tx->GetHash()));
    obj.pushKV("wtxid", ToUniValue(tx->GetWitnessHash()));
  }
  static inline void StatusInfo(UniValue &obj, const std::string &msg) {
    obj.pushKV("status", msg);
  }

  class StakeVisitor : public boost::static_visitor<UniValue> {
   public:
    UniValue operator()(const GenesisBlock &genesis) {
      UniValue obj(UniValue::VOBJ);
      BlockInfo(obj, *genesis.block);
      StatusInfo(obj, "genesis block");
      return obj;
    }
    UniValue operator()(const NotOnDisk &not_on_disk) {
      UniValue obj(UniValue::VOBJ);
      BlockInfo(obj, *not_on_disk.block);
      StatusInfo(obj, "not on disk");
      return obj;
    }
    UniValue operator()(const NoStakeFound &no_stake_found) {
      UniValue obj(UniValue::VOBJ);
      BlockInfo(obj, *no_stake_found.block);
      obj.pushKV("stake_txin", ToUniValue(no_stake_found.stake_in));
      StatusInfo(obj, "on disk, spending stake tx not found");
      return obj;
    }
    UniValue operator()(const InvalidBlock &invalid_block) {
      UniValue obj(UniValue::VOBJ);
      if (invalid_block.block) {
        BlockInfo(obj, *invalid_block.block);
        StatusInfo(obj, "on disk, block invalid: " + std::string(invalid_block.reason));
      } else {
        StatusInfo(obj, "unknown");
      }
      return obj;
    }
    UniValue operator()(const InvalidReference &invalid_reference) {
      UniValue obj(UniValue::VOBJ);
      BlockInfo(obj, *invalid_reference.block);
      obj.pushKV("stake_txin", ToUniValue(invalid_reference.stake_in));
      obj.pushKV("stake_txid", ToUniValue(invalid_reference.tx->GetHash()));
      StatusInfo(obj, "on disk, spending tx found, index invalid");
      return obj;
    }
    UniValue operator()(const StakeInfo &stake_info) {
      UniValue obj(UniValue::VOBJ);
      BlockInfo(obj, *stake_info.spending_block);
      obj.pushKV("funding_block_hash", ToUniValue(*stake_info.funding_block->phashBlock));
      obj.pushKV("funding_block_height", ToUniValue(stake_info.funding_block->nHeight));
      obj.pushKV("stake_txout", ToUniValue(stake_info.stake_out));
      obj.pushKV("stake_txin", ToUniValue(stake_info.stake_in));
      StatusInfo(obj, "ondisk, stake found");
      return obj;
    }
  };

  void TraceStake(const CBlockIndex *const start, const std::size_t max_depth, std::vector<StakeMeta> &stake_out) {

    const int start_height = start->nHeight;
    const CBlockIndex *current = start;
    stake_out.clear();

    // in case the active chain is less high than max_depth, adjust for it
    const std::size_t expected_size = std::min(std::size_t(start_height + 1), max_depth);

    // fill all expected elements with something which is well defined
    // all elements should be replaced later on, except an error occurs down the road
    stake_out.resize(expected_size, InvalidBlock{nullptr, ""});

    // Keeps track of which piece of stake is referred to by which block.
    std::multimap<uint256, std::pair<CTxIn, const CBlockIndex *>> stake_map;

    // the index in stake_out is computed based on block_height minus this offset
    const std::size_t offset = start_height - stake_out.size() + 1;
    int current_height = start_height;

    for (std::size_t i = 0; i < max_depth; ++i) {
      if (!current || !current->phashBlock) {
        break;
      }
      assert(current->nHeight == current_height && "computed height and stored height mismatch");
      const std::size_t current_ix = current_height - offset;
      const boost::optional<CBlock> block = m_block_db->ReadBlock(*current);
      if (!block) {
        stake_out[current_ix] = NotOnDisk{current};
      } else {
        if (block->vtx.empty()) {
          stake_out[current_ix] = InvalidBlock{current, "no coinbase transaction"};
        } else if (current_height == 0) {
          stake_out[current_ix] = GenesisBlock{current};
        } else {
          const CTransactionRef &coinbase = block->vtx[0];
          if (coinbase->vin.size() < 2) {
            stake_out[current_ix] = InvalidBlock{current, "no staking input"};
          } else {
            const CTxIn &stake_in = coinbase->vin[1];
            stake_out[current_ix] = NoStakeFound{current, stake_in};
            stake_map.emplace(stake_in.prevout.hash, std::make_pair(stake_in, current));
          }
        }
        // check whether any of the transactions in this block is referenced by
        // a successor as stake. Do not stop once found a piece of stake, as
        // multiple txs in this block could be references as stake.
        for (const CTransactionRef &tx : block->vtx) {
          const uint256 txid = tx->GetHash();
          const auto range = stake_map.equal_range(txid);
          const bool found = range.first != range.second;
          for (auto it = range.first; it != range.second; ++it) {
            // This transaction is being referenced as stake from a block we've come across before
            const CTxIn &stake_in = it->second.first;
            const std::uint32_t stake_out_index = stake_in.prevout.n;
            const int stake_spend_height = it->second.second->nHeight;
            const std::size_t ix = stake_spend_height - offset;
            if (stake_out_index < tx->vout.size()) {
              stake_out[ix] = StakeInfo{
                  /* funding_block= */ current,
                  /* spending_block= */ it->second.second,
                  /* stake_out= */ tx->vout[stake_out_index],
                  /* stake_in= */ stake_in};
            } else {
              stake_out[ix] = InvalidReference{
                  /* block= */ it->second.second,
                  /* tx= */ tx,
                  /* stake_in= */ stake_in};
            }
          }
          // All references to this transaction should be consumed (if there were any)
          // Remove these references from stake_map as its not ever looked at again.
          if (found) {
            stake_map.erase(txid);
          }
          // Do stop though if there are no more references to look for.
          if (stake_map.empty()) {
            break;
          }
        }
      }
      // Advance to the previous block
      current = current->pprev;
      --current_height;
    }
  }

  UniValue GetStakeInfo(const CTxIn &txin) {
    return ToUniValue(txin);
  }

  UniValue GetRewardInfo(const CTxOut &txout) {
    return ToUniValue(txout);
  }

  template <typename Iter>
  UniValue GetElementsInfo(Iter it, Iter end) {
    UniValue result(UniValue::VARR);
    for (; it != end; ++it) {
      result.push_back(ToUniValue(*it));
    }
    return result;
  }

  UniValue GetCoinbaseInfo(const CTransactionRef &tx) {
    UniValue result(UniValue::VOBJ);
    UniValue status(UniValue::VARR);
    if (!tx->IsCoinBase()) {
      status.push_back("ERROR: Not of transaction type coinbase.");
    }
    TransactionInfo(result, tx);
    switch (tx->vin.size()) {
      case 0:
        status.push_back("ERROR: No inputs.");
        break;
      case 1:
        status.push_back("ERROR: No stake.");
        break;
      default:
        result.pushKV("stake", GetStakeInfo(tx->vin[1]));
        result.pushKV("combined_stake", GetElementsInfo(tx->vin.cbegin() + 2, tx->vin.cend()));
        break;
    }
    switch (tx->vout.size()) {
      case 0:
        status.push_back("ERROR: No reward.");
        break;
      case 1:
        status.push_back("ERROR: No stake returned.");
        result.pushKV("reward", GetRewardInfo(tx->vout[0]));
        break;
      default:
        result.pushKV("reward", GetRewardInfo(tx->vout[0]));
        result.pushKV("returned_stake", GetElementsInfo(tx->vout.cbegin() + 1, tx->vout.cend()));
        break;
    }
    if (status.empty()) {
      status.push_back("OK");
    }
    result.pushKV("status", status);
    return result;
  }

  UniValue GetInitialFundsInfo(const CTransactionRef &tx) {
    UniValue result(UniValue::VOBJ);
    const CAmount amount = std::accumulate(tx->vout.cbegin(), tx->vout.cend(), CAmount(0),
                                           [](const CAmount sum, const CTxOut &out) { return sum + out.nValue; });
    TransactionInfo(result, tx);
    result.pushKV("amount", ValueFromAmount(amount));
    result.pushKV("length", static_cast<std::int64_t>(tx->vout.size()));
    result.pushKV("outputs", GetElementsInfo(tx->vout.cbegin(), tx->vout.cend()));
    return result;
  }

  UniValue GetStakeLinkInfo(const CBlockIndex &index) {
    UniValue result(UniValue::VOBJ);
    BlockInfo(result, index);
    const int height = index.nHeight;
    const boost::optional<CBlock> block = m_block_db->ReadBlock(index);
    StatusInfo(result, block ? "ondisk" : "nodata");
    if (height == 0) {
      result.pushKV("initial_funds", GetInitialFundsInfo(block->vtx[0]));
    } else if (block) {
      UniValue txs(UniValue::VARR);
      for (const CTransactionRef &tx : block->vtx) {
        txs.push_back(ToUniValue(tx->GetHash()));
      }
      result.pushKV("transactions", txs);
      if (!block->vtx.empty()) {
        result.pushKV("coinbase", GetCoinbaseInfo(block->vtx[0]));
      }
    }
    return result;
  }

  UniValue TraceChain(const CBlockIndex *const start, const std::size_t length) {
    UniValue result(UniValue::VOBJ);
    result.pushKV("start_hash", ToUniValue(*start->phashBlock));
    result.pushKV("start_height", ToUniValue(start->nHeight));
    UniValue chaininfo(UniValue::VARR);
    const CBlockIndex *current_block_index = start;
    for (std::size_t i = 0; i < length; ++i) {
      if (!current_block_index) {
        break;
      }
      chaininfo.push_back(GetStakeLinkInfo(*current_block_index));
      current_block_index = current_block_index->pprev;
    }
    result.pushKV("chain", chaininfo);
    return result;
  }

  void ReadParameters(const JSONRPCRequest &request, const CBlockIndex **start, std::size_t *length, bool *reverse = nullptr) {
    if (start) {
      AssertLockHeld(m_chain->GetLock());
      *start = m_chain->GetTip();
      if (!*start) {
        throw JSONRPCError(RPC_IN_WARMUP, "genesis block not loaded yet");
      }
      if (!request.params[0].isNull()) {
        if (request.params[0].isNum() && request.params[0].get_int() >= 0) {
          const auto height = static_cast<blockchain::Height>(request.params[0].get_int());
          *start = m_chain->AtHeight(height);
          if (!*start) {
            throw JSONRPCError(RPC_INVALID_PARAMETER,
                               strprintf("start=%d does not refer to a block in the chain (chainheight=%d)",
                                         height,
                                         m_chain->GetHeight()));
          }
        }
      }
    }
    if (length) {
      *length = default_length;
      if (!request.params[1].isNull() && request.params[1].isNum() && request.params[1].get_int() >= 0) {
        *length = static_cast<std::size_t>(request.params[1].get_int());
      }
    }
    if (reverse) {
      *reverse = default_reverse;
      if (!request.params[2].isNull() && request.params[2].isBool()) {
        *reverse = request.params[2].get_bool();
      }
    }
  }

 public:
  StakingRPCImpl(
      const Dependency<staking::ActiveChain> chain,
      const Dependency<BlockDB> block_db)
      : m_chain(chain),
        m_block_db(block_db) {}

  UniValue tracestake(const JSONRPCRequest &request) override {
    if (request.fHelp || request.params.size() > 3) {
      throw std::runtime_error(strprintf(
          "%s \"start\" \"length\"\n"
          "\n"
          "Prints detailed information about the chain of stakes.\n"
          "\n"
          "Arguments:\n"
          "  \"start\" (uint) The height to start at.\n"
          "  \"length\" (uint) Number of blocks to go back from start (defaults to %d).\n"
          "  \"reverse\" (bool) Whether to reverse output or not (defaults to %s).\n",
          __func__, default_length, default_reverse));
    }
    LOCK(m_chain->GetLock());
    const CBlockIndex *start;
    std::size_t length;
    bool reverse = default_reverse;
    ReadParameters(request, &start, &length, &reverse);
    std::vector<StakeMeta> vec;
    TraceStake(start, length, vec);
    UniValue arr(UniValue::VARR);
    StakeVisitor stake_visitor;
    auto add = [&](const StakeMeta &stake_link) { arr.push_back(boost::apply_visitor(stake_visitor, stake_link)); };
    if (reverse) {
      std::for_each(vec.crbegin(), vec.crend(), add);
    } else {
      std::for_each(vec.cbegin(), vec.cend(), add);
    }
    return arr;
  }

  UniValue tracechain(const JSONRPCRequest &request) override {
    if (request.fHelp || request.params.size() > 2) {
      throw std::runtime_error(strprintf(
          "%s \"start\" \"length\"\n"
          "\n"
          "Prints detailed information about the active chain:\n"
          "- initial funds for genesis block\n"
          "- coinbase details for all other blocks\n"
          "\n"
          "Arguments:\n"
          "  \"start\" (uint) The height to start at.\n"
          "  \"length\" (uint) Number of blocks to go back from start (defaults to %d).\n",
          __func__, default_length));
    }
    LOCK(m_chain->GetLock());
    const CBlockIndex *start;
    std::size_t length;
    ReadParameters(request, &start, &length);
    return TraceChain(start, length);
  }
};

std::unique_ptr<StakingRPC> StakingRPC::New(
    const Dependency<staking::ActiveChain> chain,
    const Dependency<::BlockDB> block_db) {
  return std::unique_ptr<StakingRPC>(new StakingRPCImpl(chain, block_db));
}

}  // namespace staking
