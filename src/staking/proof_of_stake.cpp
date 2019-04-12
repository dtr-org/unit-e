// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/proof_of_stake.h>

#include <script/script.h>
#include <script/standard.h>
#include <streams.h>

namespace staking {

std::vector<CPubKey> ExtractP2WPKHKeys(const CScriptWitness &witness) {
  const std::vector<std::vector<unsigned char>> &witness_stack = witness.stack;
  if (witness_stack.size() != 2) {
    return std::vector<CPubKey>{};
  }
  const std::vector<unsigned char> &public_key_data = witness_stack[1];
  CPubKey public_key(public_key_data.begin(), public_key_data.end());
  if (!public_key.IsFullyValid()) {
    return std::vector<CPubKey>{};
  }
  return std::vector<CPubKey>{public_key};
}

std::vector<CPubKey> ExtractP2WSHKeys(const CScriptWitness &witness) {
  const std::vector<std::vector<unsigned char>> &witness_stack = witness.stack;
  if (witness_stack.empty()) {
    return std::vector<CPubKey>{};
  }
  const std::vector<unsigned char> &script_data = witness_stack.back();
  if (script_data.empty()) {
    return std::vector<CPubKey>{};
  }
  const CScript witness_script(script_data.begin(), script_data.end());
  txnouttype type;
  std::vector<std::vector<unsigned char>> solutions;
  if (Solver(witness_script, type, solutions)) {
    switch (type) {
      case TX_PUBKEYHASH: {
        if (witness_stack.size() < 2) {
          return std::vector<CPubKey>{};
        }
        const CPubKey pub_key(witness_stack[1]);
        return std::vector<CPubKey>{pub_key};
      }
      case TX_PUBKEY: {
        const CPubKey pub_key(solutions[0]);
        return std::vector<CPubKey>{pub_key};
      }
      case TX_MULTISIG: {
        std::vector<CPubKey> result;
        // the first solution contains an OP_SMALLINTEGER with the number of signatures required
        const auto num_signatures = static_cast<std::uint8_t>(solutions.front()[0]);
        if (num_signatures != 1) {
          // stake is signed by a single proposer only and the block carries a single
          // signature of that proposer. 2-of-3 and similar multisig scenarios are not
          // allowed for staking.
          return std::vector<CPubKey>{};
        }
        // the last solution contains an OP_SMALLINTEGER with the number of pubkeys provided
        const auto num_pubkeys = static_cast<std::uint8_t>(solutions.back()[0]);
        if (num_pubkeys != solutions.size() - 2) {
          // number of pubkeys provided does not match amount required.
          return std::vector<CPubKey>{};
        }
        for (std::size_t i = 1; i < solutions.size() - 1; ++i) {
          const CPubKey key(solutions[i]);
          if (!key.IsFullyValid()) {
            return std::vector<CPubKey>{};
          }
          result.emplace_back(solutions[i]);
        }
        return result;
      }
      default:
        return std::vector<CPubKey>{};
    }
  }
  return std::vector<CPubKey>{};
}

std::vector<CPubKey> ExtractBlockSigningKeys(const CTxIn &input) {
  const CScriptWitness &witness = input.scriptWitness;
  std::vector<CPubKey> p2wpkh_keys = ExtractP2WPKHKeys(witness);
  if (p2wpkh_keys.size() == 1) {
    // p2wpkh should yield one key only.
    return p2wpkh_keys;
  }
  return ExtractP2WSHKeys(witness);
}

std::vector<CPubKey> ExtractBlockSigningKeys(const CBlock &block) {
  if (block.vtx.empty()) {
    return std::vector<CPubKey>{};
  }
  const std::vector<CTxIn> &coinbase_inputs = block.vtx[0]->vin;
  if (coinbase_inputs.size() < 2) {
    return std::vector<CPubKey>{};
  }
  return ExtractBlockSigningKeys(coinbase_inputs[1]);
}

uint256 ComputeKernelHash(const uint256 &previous_block_stake_modifier,
                          const blockchain::Time stake_block_time,
                          const uint256 &stake_txid,
                          const std::uint32_t stake_out_index,
                          const blockchain::Time target_block_time) {

  CDataStream s(SER_GETHASH, 0);

  s << previous_block_stake_modifier;
  s << stake_block_time;
  s << stake_txid;
  s << stake_out_index;
  s << target_block_time;

  return Hash(s.begin(), s.end());
}

uint256 ComputeStakeModifier(const uint256 &stake_transaction_hash,
                             const uint256 &previous_blocks_stake_modifier) {

  CDataStream s(SER_GETHASH, 0);

  s << stake_transaction_hash;
  s << previous_blocks_stake_modifier;

  return Hash(s.begin(), s.end());
}

}  // namespace staking
