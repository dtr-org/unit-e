// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <consensus/merkle.h>
#include <wallet/wallet.h>

namespace proposer {

class BlockBuilderImpl : public BlockBuilder {
 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<Settings> m_settings;

  const CTransaction BuildCoinbaseTransaction(
      const uint256 &snapshot_hash,
      const EligibleCoin &coin,
      const std::vector<COutput> &coins) const {
    CMutableTransaction tx;

    tx.SetVersion(1);
    tx.SetType(TxType::COINBASE);

    // build meta input
    {
      CScript script_sig = CScript() << CScriptNum::serialize(coin.target_height)
                                     << ToByteVector(snapshot_hash);
      tx.vin.emplace_back(uint256(), 0, script_sig);
    }

    // add stake
    tx.vin.emplace_back(coin.stake);

    // add combined stake
    CAmount combined_total = 0;
    for (const auto &c : coins) {
      const uint256 txid = c.tx->tx->GetHash();
      const auto index = static_cast<std::uint32_t>(c.tx->nIndex);
      if (txid == coin.stake.hash && index == coin.stake.n) {
        continue;
      }
      if (m_settings->stake_combine_maximum > 0) {
        combined_total += c.tx->tx->vout[index].nValue;
        if (combined_total > m_settings->stake_combine_maximum) {
          break;
        }
      }
      tx.vin.emplace_back(txid, index);
    }

    // add outputs
    if (m_settings->stake_split_threshold > 0) {

    } else {
    }

    return CTransaction(tx);
  }

 public:
  explicit BlockBuilderImpl(
      Dependency<blockchain::Behavior> blockchain_behavior,
      Dependency<Settings> settings)
      : m_blockchain_behavior(blockchain_behavior),
        m_settings(settings) {}

  std::shared_ptr<const CBlock> BuildBlock(
      const CBlockIndex &prev_block,
      const uint256 &snapshot_hash,
      const EligibleCoin &coin,
      const std::vector<COutput> &coins,
      const std::vector<CTransactionRef> &txs) const override {

    CBlock new_block;

    new_block.nVersion = 1;
    new_block.nTime = coin.target_time;
    new_block.nBits = coin.target_difficulty;
    new_block.hashPrevBlock = prev_block.GetBlockHash();
    // nonce will be removed and is not relevant in PoS, not setting it here

    // add coinbase transaction first
    new_block.vtx.emplace_back(MakeTransactionRef(BuildCoinbaseTransaction(snapshot_hash, coin, coins)));

    // add remaining transactions
    for (const auto &tx : txs) {
      new_block.vtx.emplace_back(tx);
    }

    // create tx merkle root
    {
      bool duplicate_transactions = false;
      new_block.hashMerkleRoot = BlockMerkleRoot(new_block, &duplicate_transactions);
      assert(!duplicate_transactions);
    }

    // create witness merkle root
    {
      bool duplicate_transactions = false;
      new_block.hash_witness_merkle_root = BlockWitnessMerkleRoot(new_block, &duplicate_transactions);
      assert(!duplicate_transactions);
    }

    return std::make_shared<const CBlock>(new_block);
  }
};

std::unique_ptr<BlockBuilder> BlockBuilder::New(
    Dependency<blockchain::Behavior> blockchain_behavior,
    Dependency<Settings> settings) {
  return std::unique_ptr<BlockBuilder>(new BlockBuilderImpl(blockchain_behavior, settings));
}

}  // namespace proposer
