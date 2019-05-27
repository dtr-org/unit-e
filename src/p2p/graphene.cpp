// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/graphene.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <consensus/ltor.h>
#include <p2p/graphene_hasher.h>
#include <tinyformat.h>
#include <util/scope_stopwatch.h>

namespace p2p {

boost::optional<GrapheneBlock> CreateGrapheneBlock(const CBlock &block,
                                                   size_t sender_tx_count_wo_block,
                                                   size_t receiver_tx_count,
                                                   FastRandomContext &random) {
  assert(!block.vtx.empty());

  const CTransactionRef &coinbase = block.vtx[0];
  assert(coinbase->IsCoinBase());

  FUNCTION_STOPWATCH();

  std::vector<CTransactionRef> prefilled_transactions;
  // For now we prefill only coinbase. But in the future more sophisticated
  // prefill strategies are possible
  prefilled_transactions.emplace_back(coinbase);

  const size_t non_prefilled_count = block.vtx.size() - prefilled_transactions.size();

  const GrapheneBlockParams params =
      OptimizeGrapheneBlockParams(non_prefilled_count,
                                  sender_tx_count_wo_block + non_prefilled_count,
                                  receiver_tx_count);

  // Default bloom filter implementation limits maximum size of a filter
  // and maximum hash functions. This is undesirable with graphene because it
  // ruins FPR guarantees. We accept bloom filters of any size/complexity unless
  // they allow us to create relatively small graphene blocks
  const size_t max_filter_size_bytes = std::numeric_limits<size_t>::max();
  const size_t max_hash_funcs = std::numeric_limits<size_t>::max();

  CBloomFilter bloom_filter(params.bloom_entries_num, params.bloom_filter_fpr,
                            random.rand32(), BLOOM_UPDATE_ALL, max_filter_size_bytes,
                            max_hash_funcs);

  GrapheneIblt iblt(params.expected_symmetric_difference);

  std::unordered_set<GrapheneShortHash> short_hash_cache;

  uint64_t nonce = random.rand64();
  GrapheneHasher hasher(block, nonce);

  for (size_t i = 1; i < block.vtx.size(); ++i) {
    const CTransactionRef &tx = block.vtx[i];
    const GrapheneFullHash full_hash = hasher.GetFullHash(*tx);
    const GrapheneShortHash short_hash = hasher.GetShortHash(full_hash);

    if (!short_hash_cache.emplace(short_hash).second) {
      LogPrint(BCLog::NET, "Short hash collision in graphene block %s\n", block.GetHash().GetHex());
      return boost::none;
    }

    bloom_filter.insert(full_hash);
    iblt.Insert(short_hash, {});
  }

  return GrapheneBlock(block, nonce, std::move(bloom_filter), std::move(iblt), std::move(prefilled_transactions));
}

GrapheneBlockReconstructor::GrapheneBlockReconstructor(const GrapheneBlock &graphene_block,
                                                       const TxPool &tx_pool)
    : m_header(graphene_block.header),
      m_prefilled_txs(graphene_block.prefilled_transactions),
      m_hasher(graphene_block.header, graphene_block.nonce) {

  FUNCTION_STOPWATCH();

  std::unordered_map<GrapheneShortHash, CTransactionRef> candidates;
  bool hash_collision = false;

  GrapheneIblt receiver_iblt = graphene_block.iblt.CloneEmpty();

  {
    SCOPE_STOPWATCH("Graphene tx pool enumeration");

    const std::vector<CTransactionRef> txs = tx_pool.GetTxs();
    for (const CTransactionRef &tx : txs) {
      const GrapheneFullHash full_hash = m_hasher.GetFullHash(*tx);
      const GrapheneShortHash short_hash = m_hasher.GetShortHash(full_hash);

      if (!graphene_block.bloom_filter.contains(full_hash)) {
        continue;
      }

      const auto emplace_result = candidates.emplace(short_hash, tx);
      if (!emplace_result.second) {

        const auto already_stored_hash = emplace_result.first->second->GetHash();
        LogPrint(BCLog::NET, "Hash collision while reconstructing graphene block %s: %s and %s map to %d\n",
                 graphene_block.header.GetHash().GetHex(), tx->GetHash().GetHex(), already_stored_hash.GetHex(), short_hash);

        hash_collision = true;
        break;
      }
      receiver_iblt.Insert(short_hash, {});
    };
  }

  if (hash_collision) {
    m_state = GrapheneDecodeState::CANT_DECODE_IBLT;
    return;
  }

  const GrapheneIblt iblt_diff = graphene_block.iblt - receiver_iblt;

  GrapheneIblt::TEntriesMap only_sender_has;
  GrapheneIblt::TEntriesMap only_receiver_has;

  const bool reconciled = iblt_diff.ListEntries(only_sender_has, only_receiver_has);
  if (!reconciled) {
    LogPrint(BCLog::NET, "Can not reconcile graphene block %s. Receiver iblt has %d txs, sender has %d\n",
             graphene_block.header.GetHash().GetHex(), receiver_iblt.Size(), graphene_block.iblt.Size());
    m_state = GrapheneDecodeState::CANT_DECODE_IBLT;

    return;
  }

  // Those items are unique to receiver and can not appear in this block
  for (const auto &entry : only_receiver_has) {
    candidates.erase(entry.first);
  }

  for (const auto &entry : only_sender_has) {
    m_missing_short_tx_hashes.emplace(entry.first);
  }

  for (const auto &pair : candidates) {
    m_decoded_txs.emplace_back(pair.second);
  }

  if (m_missing_short_tx_hashes.empty()) {
    m_state = GrapheneDecodeState::HAS_ALL_TXS;
  } else {
    m_state = GrapheneDecodeState::NEED_MORE_TXS;
  }
}

CBlock GrapheneBlockReconstructor::ReconstructLTOR() const {
  assert(m_state == +GrapheneDecodeState::HAS_ALL_TXS);

  CBlock block;
  block = m_header;
  std::vector<CTransactionRef> &vtx = block.vtx;

  vtx.insert(vtx.end(), m_prefilled_txs.begin(), m_prefilled_txs.end());
  vtx.insert(vtx.end(), m_decoded_txs.begin(), m_decoded_txs.end());

  for (size_t i = 0; i < vtx.size(); ++i) {
    if (vtx[i]->IsCoinBase()) {
      std::swap(vtx[0], vtx[i]);
      break;
    }
  }

  // Should be checked previously
  assert(!vtx.empty());

  ltor::SortTransactions(vtx);

  return block;
}

void GrapheneBlockReconstructor::AddMissingTxs(const std::vector<CTransactionRef> &txs) {
  assert(m_state == +GrapheneDecodeState::NEED_MORE_TXS);

  for (const auto &tx : txs) {
    const GrapheneShortHash short_hash = m_hasher.GetShortHash(*tx);

    const auto it = m_missing_short_tx_hashes.find(short_hash);
    if (it == m_missing_short_tx_hashes.end()) {
      continue;
    }

    m_decoded_txs.emplace_back(tx);
    m_missing_short_tx_hashes.erase(it);
  }

  if (m_missing_short_tx_hashes.empty()) {
    m_state = GrapheneDecodeState::HAS_ALL_TXS;
  }
}

GrapheneDecodeState GrapheneBlockReconstructor::GetState() const {
  return m_state;
}

const std::set<GrapheneShortHash> &GrapheneBlockReconstructor::GetMissingShortTxHashes() const {
  return m_missing_short_tx_hashes;
}

uint256 GrapheneBlockReconstructor::GetBlockHash() const {
  return m_header.GetHash();
}

//! \brief Computes false positive rate for bloom filter
static double ComputeFpr(const size_t symmetric_diff, const size_t receiver_excess) {
  constexpr double MAX_FPR = 0.999;
  if (receiver_excess == 0) {
    return MAX_FPR;
  }

  const double fpr = static_cast<double>(symmetric_diff) / receiver_excess;
  if (fpr > MAX_FPR) {
    return MAX_FPR;
  }

  return fpr;
}

static size_t BruteForceSymDif(const size_t all_receiver_txs,
                               const size_t receiver_excess,
                               const size_t bloom_entries) {
  FUNCTION_STOPWATCH();

  size_t min_achieved_size = std::numeric_limits<size_t>::max();
  size_t best_sym_diff = 2;

  static const size_t iblt_entry_size =
      GetSerializeSize(GrapheneIblt::IBLTEntry(), PROTOCOL_VERSION);

  for (size_t sym_diff = best_sym_diff; sym_diff <= all_receiver_txs; ++sym_diff) {
    const double fpr = ComputeFpr(sym_diff, receiver_excess);

    const size_t bloom_size = CBloomFilter::ComputeEntriesSize(bloom_entries, fpr);
    const size_t iblt_entries = GrapheneIblt::ComputeNumberOfEntries(sym_diff);
    const size_t iblt_size = iblt_entries * iblt_entry_size;

    const size_t size = iblt_size + bloom_size;
    if (size < min_achieved_size) {
      min_achieved_size = size;
      best_sym_diff = sym_diff;
    }
  }

  return best_sym_diff;
}

GrapheneBlockParams OptimizeGrapheneBlockParams(const size_t block_txs,
                                                const size_t all_sender_txs,
                                                const size_t all_receiver_txs) {
  // This function uses some heuristics to determine optimal graphene block
  // parameters, you can find some explanations here
  // https://gist.github.com/bissias/561151fef0b98f6e4d8813a08aefe349

  // All sender txs should include block txs
  assert(all_sender_txs >= block_txs);

  size_t receiver_excess = 0;
  if (all_receiver_txs > block_txs) {
    receiver_excess = all_receiver_txs - block_txs;
  }
  const size_t sender_excess = all_sender_txs - block_txs;

  receiver_excess = std::max(receiver_excess, sender_excess);
  receiver_excess = std::min(receiver_excess, all_receiver_txs);
  receiver_excess = std::max<decltype(receiver_excess)>(receiver_excess, 1);

  size_t missing = 1;
  if (block_txs > (all_receiver_txs - receiver_excess)) {
    missing = block_txs - (all_receiver_txs - receiver_excess);
  }

  size_t sym_diff = missing;

  const size_t bloom_entries = std::max<size_t>(1, block_txs);

  if (sym_diff <= all_receiver_txs + missing && all_receiver_txs < GRAPHENE_TOO_BIG_TXPOOL) {
    sym_diff = BruteForceSymDif(all_receiver_txs, receiver_excess, bloom_entries);
  }

  const double fpr = ComputeFpr(sym_diff, receiver_excess);

  sym_diff += missing;

  return GrapheneBlockParams(sym_diff, bloom_entries, fpr);
}

GrapheneBlockParams::GrapheneBlockParams(
    const size_t expected_symmetric_difference,
    const size_t bloom_entries_num,
    const double bloom_filter_fpr)
    : expected_symmetric_difference(expected_symmetric_difference),
      bloom_entries_num(bloom_entries_num),
      bloom_filter_fpr(bloom_filter_fpr) {}

}  // namespace p2p
