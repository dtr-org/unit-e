// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_GRAPHENE_HASHER_H
#define UNITE_P2P_GRAPHENE_HASHER_H

#include <cstdint>

#include <p2p/graphene_common.h>
#include <primitives/block.h>

namespace p2p {

class GrapheneHasher {
 public:
  GrapheneHasher(const CBlockHeader &header, uint64_t nonce);
  GrapheneShortHash GetShortHash(const uint256 &full_hash) const;
  GrapheneShortHash GetShortHash(const CTransaction &tx) const;
  GrapheneFullHash GetFullHash(const CTransaction &tx) const;

 private:
  uint64_t m_k0;
  uint64_t m_k1;
};

}  // namespace p2p

#endif  //UNITE_P2P_GRAPHENE_HASHER_H
