// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <consensus/merkle.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <crypto/common.h>

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

void CBlock::ComputeMerkleTrees() {
    bool mutated = false;
    hashMerkleRoot = BlockMerkleRoot(*this, &mutated);
    assert(!mutated && "merkle tree contained duplicates");
    hash_finalizer_commits_merkle_root = BlockFinalizerCommitsMerkleRoot(*this);
    hash_witness_merkle_root = BlockWitnessMerkleRoot(*this, &mutated);
    assert(!mutated && "witness merkle tree contained duplicates");
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock("
                   "hash=%s, "
                   "ver=0x%08x, "
                   "hashPrevBlock=%s, "
                   "hashMerkleRoot=%s, "
                   "hash_witness_merkle_root=%s, "
                   "hash_finalizer_commits_merkle_root=%s, "
                   "nTime=%u, "
                   "nBits=%08x, "
                   "vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        hash_witness_merkle_root.ToString(),
        hash_finalizer_commits_merkle_root.ToString(),
        nTime, nBits,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
