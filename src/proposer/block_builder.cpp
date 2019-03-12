// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <consensus/merkle.h>
#include <key.h>
#include <pubkey.h>
#include <staking/proof_of_stake.h>

#include <numeric>

#define Log(MSG) LogPrint(BCLog::PROPOSING, "%s: " MSG "\n", __func__)

namespace proposer {

class BlockBuilderImpl : public BlockBuilder {
 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<Settings> m_settings;

  std::vector<CAmount> SplitAmount(const CAmount amount, const CAmount threshold) const {
    auto number_of_pieces = amount / threshold;
    if (amount % threshold > 0) {
      // if spend can not be spread evenly we need one more to fit the rest
      ++number_of_pieces;
    }
    // in order to not create a piece of dust of size (spend % threshold), try
    // to spread evenly by forming pieces of size (spend / number_of_pieces) each
    std::vector<CAmount> pieces(static_cast<std::size_t>(number_of_pieces),
                                amount / number_of_pieces);
    auto number_of_full_pieces = amount % number_of_pieces;
    // distribute the remaining units not spread yet
    for (auto i = 0; i < number_of_full_pieces; ++i) {
      ++pieces[i];
    }
    return pieces;
  }

  bool SignBlock(CBlock &block, const staking::StakingWallet &wallet) const {
    const std::vector<CPubKey> keys = staking::ExtractBlockSigningKeys(block);
    if (keys.empty()) {
      Log("Could not extract staking key(s) from block.");
      return false;
    }
    for (const CPubKey &pubkey : keys) {
      const auto key = wallet.GetKey(pubkey);
      if (!key) {
        Log("No private key for public key.");
        continue;
      }
      const uint256 block_hash = block.GetHash();
      if (!key->Sign(block_hash, block.signature)) {
        Log("Could not create block signature.");
        continue;
      }
      Log("Created block signature.");
      return true;
    }
    Log("Could not sign block, no key could be used for signing.");
    return false;
  }

 public:
  explicit BlockBuilderImpl(
      Dependency<blockchain::Behavior> blockchain_behavior,
      Dependency<Settings> settings)
      : m_blockchain_behavior(blockchain_behavior),
        m_settings(settings) {}

  const CTransactionRef BuildCoinbaseTransaction(
      const uint256 &snapshot_hash,
      const EligibleCoin &eligible_coin,
      const staking::CoinSet &coins,
      const CAmount fees,
      staking::StakingWallet &wallet) const override {
    CMutableTransaction tx;

    // UNIT-E TODO: Restore BIP-9 versioning here
    tx.SetVersion(1);
    tx.SetType(TxType::COINBASE);

    // build meta input
    {
      CScript script_sig = CScript() << CScriptNum::serialize(eligible_coin.target_height)
                                     << ToByteVector(snapshot_hash);
      tx.vin.emplace_back(uint256(), 0, script_sig);
    }

    // add stake
    tx.vin.emplace_back(eligible_coin.utxo.GetOutPoint());

    // add combined stake - we already include the eligible coin and its amount.
    CAmount combined_total = eligible_coin.utxo.GetAmount();
    for (const auto &coin : coins) {
      if (coin == eligible_coin.utxo) {
        // if it's the staking coin we already included it in tx.vin so we
        // can skip here. It is already included in combined_total.
        continue;
      }
      const CAmount new_total = combined_total + coin.GetAmount();
      if (m_settings->stake_combine_maximum > 0 && new_total > m_settings->stake_combine_maximum) {
        // stake combination does not break here, but it continues here. This
        // way the order of the coins does not matter. If there is another coin
        // later on which actually fits stake_combine_maximum it might still
        // be included.
        continue;
      }
      combined_total = new_total;
      tx.vin.emplace_back(coin.GetOutPoint());
    }

    const CAmount reward = fees + eligible_coin.reward;

    // Send fees and block reward to the reward_address set, if one is
    // configured. If an empty block is proposed and there's no block reward
    // (which happens after the finite supply limit is reached)
    // then there is no reward at all. The reward output will nevertheless
    // be added with an amount of zero.
    const CScript reward_script = m_settings->reward_destination && reward > 0
                                      ? GetScriptForDestination(*m_settings->reward_destination)
                                      : eligible_coin.utxo.GetScriptPubKey();
    tx.vout.emplace_back(reward, reward_script);

    const CAmount threshold = m_settings->stake_split_threshold;
    if (threshold > 0 && combined_total > threshold) {
      const std::vector<CAmount> pieces = SplitAmount(combined_total, threshold);
      for (const CAmount amount : pieces) {
        tx.vout.emplace_back(amount, eligible_coin.utxo.GetScriptPubKey());
      }
    } else {
      tx.vout.emplace_back(combined_total, eligible_coin.utxo.GetScriptPubKey());
    }

    assert(std::accumulate(tx.vout.begin(), tx.vout.end(), CAmount(0),
                           [](const CAmount sum, const CTxOut &tx_out) -> CAmount {
                             return sum + tx_out.nValue;
                           }) == combined_total + reward);

    // sign inputs
    {
      LOCK(wallet.GetLock());
      if (!wallet.SignCoinbaseTransaction(tx)) {
        Log("Failed to sign coinbase transaction.");
        return nullptr;
      }
    }

    return std::make_shared<CTransaction>(tx);
  }

  std::shared_ptr<const CBlock> BuildBlock(
      const CBlockIndex &prev_block,
      const uint256 &snapshot_hash,
      const EligibleCoin &coin,
      const staking::CoinSet &coins,
      const std::vector<CTransactionRef> &txs,
      const CAmount fees,
      staking::StakingWallet &wallet) const override {

    const std::shared_ptr<CBlock> new_block = std::make_shared<CBlock>();

    new_block->nVersion = 1;
    new_block->nTime = coin.target_time;
    new_block->nBits = coin.target_difficulty;
    new_block->hashPrevBlock = prev_block.GetBlockHash();
    // nonce will be removed and is not relevant in PoS, not setting it here

    // add coinbase transaction first
    const CTransactionRef coinbase_transaction =
        BuildCoinbaseTransaction(snapshot_hash, coin, coins, fees, wallet);
    if (!coinbase_transaction) {
      Log("Failed to create coinbase transaction.");
      return nullptr;
    }
    new_block->vtx.emplace_back(coinbase_transaction);

    // add remaining transactions
    new_block->vtx.insert(new_block->vtx.end(), txs.begin(), txs.end());

    // create tx merkle root
    {
      bool duplicate_transactions = false;
      new_block->hashMerkleRoot = BlockMerkleRoot(*new_block, &duplicate_transactions);
      if (duplicate_transactions) {
        Log("Duplicate transactions detected while constructing merkle tree.");
        return nullptr;
      }
    }

    // create witness merkle root
    {
      bool duplicate_transactions = false;
      new_block->hash_witness_merkle_root = BlockWitnessMerkleRoot(*new_block, &duplicate_transactions);
      if (duplicate_transactions) {
        Log("Duplicate transactions detected while constructing witness merkle tree.");
        return nullptr;
      }
    }

    if (!SignBlock(*new_block, wallet)) {
      Log("Failed to sign block.");
      return nullptr;
    }
    return new_block;
  }
};

std::unique_ptr<BlockBuilder> BlockBuilder::New(
    Dependency<blockchain::Behavior> blockchain_behavior,
    Dependency<Settings> settings) {
  return std::unique_ptr<BlockBuilder>(new BlockBuilderImpl(blockchain_behavior, settings));
}

}  // namespace proposer
