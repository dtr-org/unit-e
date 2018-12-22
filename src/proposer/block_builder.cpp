// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/block_builder.h>

#include <consensus/merkle.h>
#include <fixed_vector.h>
#include <wallet/wallet.h>

#define Log(MSG) LogPrint(BCLog::PROPOSING, "%s: " MSG "\n", __func__)

namespace proposer {

class BlockBuilderImpl : public BlockBuilder {
 private:
  Dependency<blockchain::Behavior> m_blockchain_behavior;
  Dependency<Settings> m_settings;

  const boost::optional<CTransaction> BuildCoinbaseTransaction(
      const uint256 &snapshot_hash,
      const EligibleCoin &coin,
      const std::vector<COutput> &coins,
      const CAmount fees,
      CWallet &wallet) const {
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

    // add combined stake - we already include the eligible coin and its amount.
    CAmount combined_total = coin.amount;
    for (const auto &c : coins) {
      const uint256 txid = c.tx->tx->GetHash();
      const auto index = static_cast<std::uint32_t>(c.tx->nIndex);
      if (txid == coin.stake.hash && index == coin.stake.n) {
        // if it's the staking coin we already included it in tx.vin so we
        // can skip here. It is already included in combined_total.
        continue;
      }
      const CTxOut &prev_out = c.tx->tx->vout[index];
      const CAmount amount = prev_out.nValue;
      combined_total += amount;
      if (m_settings->stake_combine_maximum > 0) {
        if (combined_total > m_settings->stake_combine_maximum) {
          // if the combined_total exceeds the stake combination maximum then
          // the coin should not be included. Since it's already counted towards
          // combined_total it's being subtracted away again.
          combined_total -= amount;
          // stake combination does not break here, but it continues here. This
          // way the order ot the coins does not matter. If there is another coin
          // later on which actually fits stake_combine_maximum it might still
          // beincluded.
          continue;
        }
      }
      tx.vin.emplace_back(txid, index);
    }

    // destination to send stake + reward to
    const CPubKey pub_key;
    const CTxDestination destination = WitnessV0KeyHash(pub_key.GetID());
    const CScript script_pub_key = GetScriptForDestination(destination);

    // add outputs
    const CAmount spend = combined_total + coin.reward + fees;
    const CAmount threshold = m_settings->stake_split_threshold;
    if (threshold > 0 && spend > threshold) {
      const std::vector<CAmount> pieces = SplitAmount(spend, threshold);
      for (const CAmount amount : pieces) {
        tx.vout.emplace_back(amount, script_pub_key);
      }
    } else {
      tx.vout.emplace_back(spend, script_pub_key);
    }

    // sign inputs
    if (!wallet.SignTransaction(tx)) {
      Log("Failed to sign coinbase transaction.");
      return boost::none;
    }

    return CTransaction(tx);
  }

  std::vector<CAmount> SplitAmount(const CAmount amount, const CAmount threshold) const {
    auto number_of_pieces = amount / threshold;
    if (amount % threshold > 0) {
      // it spend can not be spread evenly we need one more to fit the rest
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

  bool SignBlock(CBlock &block, const CKeyStore &key_store) const {
    const boost::optional<CPubKey> key = m_blockchain_behavior->ExtractBlockSigningKey(block);
    if (!key) {
      Log("Could not extract staking key from block.");
      return false;
    }
    const CKeyID key_id = key->GetID();
    CKey private_key;
    if (!key_store.GetKey(key_id, private_key)) {
      Log("No private key for public key.");
      return false;
    }
    const uint256 block_hash = block.GetHash();
    if (!private_key.Sign(block_hash, block.signature)) {
      Log("Could not create block signature.");
      return false;
    }
    return true;
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
      const std::vector<CTransactionRef> &txs,
      const CAmount fees,
      CWallet &wallet) const override {

    CBlock new_block;

    new_block.nVersion = 1;
    new_block.nTime = coin.target_time;
    new_block.nBits = coin.target_difficulty;
    new_block.hashPrevBlock = prev_block.GetBlockHash();
    // nonce will be removed and is not relevant in PoS, not setting it here

    // add coinbase transaction first
    const boost::optional<CTransaction> coinbase_transaction =
        BuildCoinbaseTransaction(snapshot_hash, coin, coins, fees, wallet);
    if (!coinbase_transaction) {
      Log("Failed to create coinbase transaction.");
      return nullptr;
    }
    new_block.vtx.emplace_back(MakeTransactionRef(*coinbase_transaction));

    // add remaining transactions
    for (const auto &tx : txs) {
      new_block.vtx.emplace_back(tx);
    }

    // create tx merkle root
    {
      bool duplicate_transactions = false;
      new_block.hashMerkleRoot = BlockMerkleRoot(new_block, &duplicate_transactions);
      if (duplicate_transactions) {
        Log("Duplicate transactions detected while contructing merkle tree.");
        return nullptr;
      }
    }

    // create witness merkle root
    {
      bool duplicate_transactions = false;
      new_block.hash_witness_merkle_root = BlockWitnessMerkleRoot(new_block, &duplicate_transactions);
      if (duplicate_transactions) {
        Log("Duplicate transactions detected while constructing witness merkle tree.");
        return nullptr;
      }
    }

    if (!SignBlock(new_block, wallet)) {
      Log("Failed to sign block.");
      return nullptr;
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
