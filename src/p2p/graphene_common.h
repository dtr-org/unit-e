// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_GRAPHENE_COMMON_H
#define UNITE_P2P_GRAPHENE_COMMON_H

#include <consensus/consensus.h>
#include <iblt.h>
#include <net.h>
#include <netmessagemaker.h>
#include <primitives/transaction.h>
#include <cstdint>

namespace p2p {

constexpr size_t MAX_TRANSACTIONS_IN_GRAPHENE_BLOCK =
    MAX_BLOCK_WEIGHT / MIN_SERIALIZABLE_TRANSACTION_WEIGHT;

// Do not use brute-force optimization if receiver mempool exceeds this value
constexpr size_t GRAPHENE_TOO_BIG_TXPOOL = 10000000;

// Do not bother creating graphene block if block has less than so many transactions
// Estimation:
// Minimal IBLT size is 252 bytes (sym diff=2), minimal bloom filter size is 10 bytes
// Compact block uses 6 bytes per transaction.
// 262 / 6 ~= 44
constexpr size_t MIN_TRANSACTIONS_IN_GRAPHENE_BLOCK = 44;

using GrapheneShortHash = uint64_t;
struct GrapheneFullHash : uint256 {
  explicit GrapheneFullHash(const uint256 &hash) : uint256(hash) {}
};
using GrapheneIblt = IBLT<GrapheneShortHash, 0>;

template <typename T>
void PushMessage(CNode &to, const char *message, T &&data) {
  const CNetMsgMaker msg_maker(to.GetSendVersion());
  g_connman->PushMessage(&to, msg_maker.Make(message, std::forward<T>(data)));
}

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_COMMON_H
