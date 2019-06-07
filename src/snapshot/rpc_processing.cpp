// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/rpc_processing.h>

#include <chainparams.h>
#include <rpc/server.h>
#include <serialize.h>
#include <snapshot/creator.h>
#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <snapshot/snapshot_index.h>
#include <snapshot/snapshot_validation.h>
#include <streams.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <validation.h>
#include <version.h>

namespace snapshot {

UniValue SnapshotNode(const uint256 &snapshot_hash) {
  UniValue node(UniValue::VOBJ);
  node.pushKV("snapshot_hash", snapshot_hash.GetHex());

  LOCK2(cs_main, cs_snapshot);

  std::unique_ptr<Indexer> idx = SnapshotIndex::OpenSnapshot(snapshot_hash);
  if (!idx) {
    node.pushKV("valid", false);
    return node;
  }

  const snapshot::SnapshotHeader &snapshot_header = idx->GetSnapshotHeader();
  node.pushKV("valid", true);
  node.pushKV("block_hash", snapshot_header.block_hash.GetHex());
  node.pushKV("block_height", mapBlockIndex[snapshot_header.block_hash]->nHeight);
  node.pushKV("stake_modifier", snapshot_header.stake_modifier.GetHex());
  node.pushKV("chain_work", snapshot_header.chain_work.GetHex());
  node.pushKV("total_utxo_subsets", snapshot_header.total_utxo_subsets);

  uint64_t outputs = 0;
  Iterator iter(std::move(idx));
  while (iter.Valid()) {
    outputs += iter.GetUTXOSubset().outputs.size();
    iter.Next();
  }
  node.pushKV("total_outputs", outputs);

  return node;
}

UniValue listsnapshots(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 0) {
    throw std::runtime_error(
        "listsnapshots\n"
        "\nLists all snapshots.\n"
        "\nExamples:\n" +
        HelpExampleCli("listsnapshots", "") +
        HelpExampleRpc("listsnapshots", ""));
  }

  UniValue list_nodes(UniValue::VARR);
  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    UniValue node = SnapshotNode(p.snapshot_hash);
    node.pushKV("snapshot_finalized", p.finalized);
    list_nodes.push_back(node);
  }

  return list_nodes;
}

UniValue getblocksnapshot(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 1) {
    throw std::runtime_error(
        "getblocksnapshot (<blockhash>)\n"
        "\nReturns the snapshot hash of the block.\n"
        "\nArguments:\n"
        "1. blockhash (hex, optional) block hash to lookup. If missing, the top is used. "
        "\nExamples:\n" +
        HelpExampleCli("getblocksnapshot", "0000000000d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") +
        HelpExampleRpc("getblocksnapshot", "0000000000d5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03"));
  }

  UniValue root_node(UniValue::VOBJ);

  LOCK(cs_main);

  const CBlockIndex *block_index = chainActive.Tip();
  if (!request.params.empty()) {
    const uint256 block_hash = uint256S(request.params[0].get_str());
    const auto it = mapBlockIndex.find(block_hash);
    if (it == mapBlockIndex.end()) {
      root_node.pushKV("error", "invalid block hash");
      return root_node;
    }
    block_index = it->second;
  }

  uint256 snapshot_hash;
  if (block_index == chainActive.Tip()) {
    snapshot_hash = pcoinsTip->GetSnapshotHash().GetHash(block_index->stake_modifier,
                                                         ArithToUint256(block_index->nChainWork));
  } else {
    CBlockIndex *parent = chainActive[block_index->nHeight + 1];
    if (parent->pprev != block_index) {
      // requested block is not in the active chain
      bool found = false;
      for (const auto &p : mapBlockIndex) {
        if (p.second->pprev == block_index) {
          parent = p.second;
          found = true;
          break;
        }
      }

      if (!found) {
        root_node.pushKV("error", "can't retrieve snapshot hash of the fork");
        return root_node;
      }
    }

    CBlock block;
    if (!ReadBlockFromDisk(block, parent, ::Params().GetConsensus())) {
      root_node.pushKV("error", "can't read block from disk");
      return root_node;
    }

    if (!ReadSnapshotHashFromTx(*block.vtx[0].get(), snapshot_hash)) {
      root_node.pushKV("error", "block doesn't contain snapshot hash");
      return root_node;
    }
  }

  UniValue node = SnapshotNode(snapshot_hash);
  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    if (p.snapshot_hash == snapshot_hash) {
      node.pushKV("snapshot_finalized", p.finalized);
      return node;
    }
  }

  node.pushKV("snapshot_deleted", true);
  node.pushKV("block_hash", block_index->GetBlockHash().GetHex());
  return node;
}

UniValue deletesnapshot(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 1) {
    throw std::runtime_error(
        "deletesnapshot (<snapshothash>)\n"
        "\nDeletes snapshot from disk.\n"
        "\nArguments:\n"
        "1. snapshothash (hex, required) hash of the snapshot to delete"
        "\nExamples:\n" +
        HelpExampleCli("deletesnapshot", "34aa7d3aabd5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") +
        HelpExampleRpc("deletesnapshot", "34aa7d3aabd5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03"));
  }

  LOCK(cs_snapshot);

  const uint256 snapshot_hash = uint256S(request.params[0].get_str());
  SnapshotIndex::DeleteSnapshot(snapshot_hash);

  UniValue root(UniValue::VOBJ);
  root.pushKV("snapshot_hash", snapshot_hash.GetHex());
  return root;
}

UniValue calcsnapshothash(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() < 4 || request.params.size() > 5) {
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << std::vector<UTXO>(1);
    std::string inputs = HexStr(stream);
    stream.clear();

    stream << std::vector<UTXO>(1);
    std::string outputs = HexStr(stream);
    stream.clear();

    stream << uint256S("aa");
    std::string stake_modifier = HexStr(stream);
    stream.clear();

    stream << uint256S("bb");
    std::string chain_work = HexStr(stream);
    stream.clear();

    SnapshotHash hash;
    stream << hash.GetData();
    std::string snapshot_data = HexStr(stream);
    stream.clear();

    std::string example = inputs + " " + outputs + " " + stake_modifier + " " + chain_work + " " + snapshot_data;
    throw std::runtime_error(
        "calcsnapshothash\n"
        "\nReturns snapshot hash and its data after arithmetic calculations\n"
        "\nArguments:\n"
        "1. \"inputs\" (hex, required) serialized UTXOs to subtract.\n"
        "2. \"outputs\" (hex, required) serialized UTXOs to add.\n"
        "3. \"stake_modifier\" (hex, required) stake modifier of the current block\n"
        "4. \"chain_work\" (hex, required) chain work of the current block\n"
        "5. \"snapshotData\" (hex, optional) initial snapshot data.\n"
        "\nExamples:\n" +
        HelpExampleCli("calcsnapshothash", example) +
        HelpExampleRpc("calcsnapshothash", example));
  }

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  for (std::size_t i = 0; i < 2; ++i) {
    std::vector<uint8_t> data = ParseHex(request.params[i].get_str());
    stream.write(reinterpret_cast<char *>(data.data()), data.size());
  }

  std::vector<UTXO> inputs;
  std::vector<UTXO> outputs;
  stream >> inputs;
  stream >> outputs;

  const uint256 stake_modifier = uint256S(request.params[2].get_str());
  const uint256 chain_work = uint256(ParseHex(request.params[3].get_str()));

  SnapshotHash hash;
  if (request.params.size() == 5) {
    hash = SnapshotHash(ParseHex(request.params[4].get_str()));
  }

  for (const auto &in : inputs) {
    hash.SubtractUTXO(in);
  }

  for (const auto &out : outputs) {
    hash.AddUTXO(out);
  }

  UniValue root(UniValue::VOBJ);
  root.pushKV("hash", HexStr(hash.GetHash(stake_modifier, chain_work)));
  root.pushKV("data", HexStr(hash.GetData()));
  return root;
}

UniValue gettipsnapshot(const JSONRPCRequest &request) {
  if (request.fHelp) {
    throw std::runtime_error(
        "gettipsnapshot\n"
        "\nReturns the snapshot hash of the tip\n"
        "\nExamples:\n" +
        HelpExampleCli("gettipsnapshot", "") +
        HelpExampleRpc("gettipsnapshot", ""));
  }

  UniValue root(UniValue::VOBJ);

  LOCK(cs_main);
  SnapshotHash hash = pcoinsTip->GetSnapshotHash();
  uint256 sm = chainActive.Tip()->stake_modifier;
  uint256 cw = ArithToUint256(chainActive.Tip()->nChainWork);
  root.pushKV("hash", HexStr(hash.GetHash(sm, cw)));
  root.pushKV("data", HexStr(hash.GetData()));

  return root;
}

UniValue getrawsnapshot(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() != 1) {
    throw std::runtime_error(
        "getrawsnapshot\n"
        "\nReturns hex string that contains snapshot data\n"
        "\nArguments:\n"
        "1. \"snapshothash\" (hex, required) snapshot that must be returned.\n"
        "\nExamples:\n" +
        HelpExampleCli("getrawsnapshot", "34aa7d3aabd5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03") +
        HelpExampleRpc("getrawsnapshot", "34aa7d3aabd5df086d0ff0b110fbd9d21bb4fc7163af34d08286a2e846f6be03"));
  }

  LOCK(cs_snapshot);

  uint256 snapshot_hash = uint256S(request.params[0].get_str());
  std::unique_ptr<Indexer> idx = SnapshotIndex::OpenSnapshot(snapshot_hash);
  if (!idx) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Snapshot was not found");
  }

  Snapshot snapshot;
  snapshot.snapshot_hash = idx->GetSnapshotHeader().snapshot_hash;
  snapshot.utxo_subset_index = 0;

  Iterator iter(std::move(idx));
  while (iter.Valid()) {
    snapshot.utxo_subsets.emplace_back(iter.GetUTXOSubset());
    iter.Next();
  }

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  stream << snapshot;
  return HexStr(stream.begin(), stream.end());
}

// clang-format off
static const CRPCCommand commands[] = {
    // category   name                actor (function)   argNames
    // --------   ------------------  -----------------  --------
    { "snapshot", "deletesnapshot",   &deletesnapshot,   {"snapshothash"} },
    { "snapshot", "getblocksnapshot", &getblocksnapshot, {"blockhash"} },
    { "snapshot", "listsnapshots",    &listsnapshots,    {""} },
    { "snapshot", "gettipsnapshot",   &gettipsnapshot,   {}},
    { "snapshot", "calcsnapshothash", &calcsnapshothash, {}},
    { "snapshot", "getrawsnapshot",   &getrawsnapshot,   {"snapshothash"}},
};
// clang-format on

void RegisterRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
}  // namespace snapshot
