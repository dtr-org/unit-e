// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/coin.h>

#include <util.h>

#include <chain.h>
#include <tinyformat.h>

namespace staking {

Coin::Coin(const CBlockIndex *containing_block, const COutPoint &out_point, const CTxOut &tx_out)
    : containing_block(containing_block), out_point(out_point), tx_out(tx_out) {}

const uint256 &Coin::GetBlockHash() const {
  return *containing_block->phashBlock;
}

blockchain::Time Coin::GetBlockTime() const {
  return containing_block->nTime;
}

blockchain::Height Coin::GetHeight() const {
  return static_cast<blockchain::Height>(containing_block->nHeight);
}

std::uint32_t Coin::GetOutputIndex() const {
  return out_point.n;
}

CAmount Coin::GetAmount() const {
  return tx_out.nValue;
}

const CScript &Coin::GetScriptPubKey() const {
  return tx_out.scriptPubKey;
}

const uint256 &Coin::GetTransactionHash() const {
  return out_point.hash;
}

const COutPoint &Coin::GetOutPoint() const {
  return out_point;
}

std::string Coin::ToString() const {
  return tfm::format("Coin(txid=%s,index=%d,amount=%d,height=%d)",
                     util::to_string(GetTransactionHash()), GetOutputIndex(), GetAmount(), GetHeight());
}

bool CoinByAmountComparator::operator()(const Coin &left, const Coin &right) const {
  if (left.GetAmount() > right.GetAmount()) {
    return true;
  }
  if (left.GetAmount() < right.GetAmount()) {
    return false;
  }
  if (left.GetHeight() < right.GetHeight()) {
    return true;
  }
  if (left.GetHeight() > right.GetHeight()) {
    return false;
  }
  if (left.GetTransactionHash() < right.GetTransactionHash()) {
    return true;
  }
  if (left.GetTransactionHash() != right.GetTransactionHash()) {
    return false;
  }
  return left.GetOutputIndex() < right.GetOutputIndex();
}

}  // namespace staking
