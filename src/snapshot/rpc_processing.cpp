// Copyright (c) 2018 The Unit-e developers
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
#include <utilstrencodings.h>
#include <validation.h>
#include <version.h>

namespace snapshot {

UniValue SnapshotNode(const uint256 &snapshotHash) {
  UniValue node(UniValue::VOBJ);
  node.push_back(Pair("snapshot_hash", snapshotHash.GetHex()));
  std::unique_ptr<Indexer> idx = Indexer::Open(snapshotHash);
  if (!idx) {
    node.push_back(Pair("valid", false));
    return node;
  }

  node.push_back(Pair("valid", true));
  node.push_back(Pair("block_hash", idx->GetMeta().block_hash.GetHex()));
  node.push_back(Pair("block_height", mapBlockIndex[idx->GetMeta().block_hash]->nHeight));
  node.push_back(Pair("stake_modifier", idx->GetMeta().stake_modifier.GetHex()));
  node.push_back(Pair("total_utxo_subsets", idx->GetMeta().total_utxo_subsets));

  uint64_t outputs = 0;
  Iterator iter(std::move(idx));
  while (iter.Valid()) {
    outputs += iter.GetUTXOSubset().outputs.size();
    iter.Next();
  }
  node.push_back(Pair("total_outputs", outputs));

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

  UniValue listNode(UniValue::VARR);
  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    UniValue node = SnapshotNode(p.snapshot_hash);
    node.push_back(Pair("snapshot_finalized", p.finalized));
    listNode.push_back(node);
  }

  return listNode;
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

  UniValue rootNode(UniValue::VOBJ);

  CBlockIndex *blockIndex = chainActive.Tip();
  if (!request.params.empty()) {
    uint256 blockHash = uint256S(request.params[0].get_str());
    const auto it = mapBlockIndex.find(blockHash);
    if (it == mapBlockIndex.end()) {
      rootNode.push_back(Pair("error", "invalid block hash"));
      return rootNode;
    }
    blockIndex = it->second;
  }

  uint256 snapshotHash;
  if (blockIndex == chainActive.Tip()) {
    snapshotHash = pcoinsTip->GetSnapshotHash().GetHash(blockIndex->bnStakeModifier);
  } else {
    CBlockIndex *parent = chainActive[blockIndex->nHeight + 1];
    if (parent->pprev != blockIndex) {
      // requested block is not in the active chain
      bool found = false;
      for (const auto &p : mapBlockIndex) {
        if (p.second->pprev == blockIndex) {
          parent = p.second;
          found = true;
          break;
        }
      }

      if (!found) {
        rootNode.push_back(Pair("error", "can't retrieve snapshot hash of the fork"));
        return rootNode;
      }
    }

    CBlock block;
    if (!ReadBlockFromDisk(block, parent, ::Params().GetConsensus())) {
      rootNode.push_back(Pair("error", "can't read block from disk"));
      return rootNode;
    }

    if (!ReadSnapshotHashFromTx(*block.vtx[0].get(), snapshotHash)) {
      rootNode.push_back(Pair("error", "block doesn't contain snapshot hash"));
      return rootNode;
    }
  }

  UniValue node = SnapshotNode(snapshotHash);
  for (const Checkpoint &p : GetSnapshotCheckpoints()) {
    if (p.snapshot_hash == snapshotHash) {
      node.push_back(Pair("snapshot_finalized", p.finalized));
      return node;
    }
  }

  node.push_back(Pair("snapshot_deleted", true));
  node.push_back(Pair("block_hash", blockIndex->GetBlockHash().GetHex()));
  return node;
}

UniValue createsnapshot(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 1) {
    throw std::runtime_error(
        "createsnapshot (<maxutxosubsets>)\n"
        "\nCreates the snapshot of the UTXO subsets on the disk.\n"
        "\nArguments:\n"
        "1. maxutxosubsets (numeric, optional) Maximum UTXO subsets to dump into the file"
        "\nExamples:\n" +
        HelpExampleCli("createsnapshot", "10") + HelpExampleRpc("createsnapshot", "10"));
  }

  FlushStateToDisk();

  Creator creator(pcoinsdbview.get());
  if (!request.params.empty()) {
    creator.m_maxUTXOSubsets = static_cast<uint64_t>(request.params[0].get_int64());
  }

  CreationInfo info = creator.Create();
  if (info.status != +Status::OK) {
    UniValue rootNode(UniValue::VOBJ);
    switch (info.status) {
      case Status::WRITE_ERROR:
        rootNode.push_back(Pair("error", "can't write to any *.dat files"));
        break;
      case Status::CALC_SNAPSHOT_HASH_ERROR:
        rootNode.push_back(Pair("error", "can't calculate hash of the snapshot"));
        break;
      default:
        rootNode.push_back(Pair("error", "unknown error happened during creating snapshot"));
    }
    return rootNode;
  }

  return SnapshotNode(info.indexer_meta.snapshot_hash);
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

  uint256 snapshotHash = uint256S(request.params[0].get_str());
  SnapshotIndex::DeleteSnapshot(snapshotHash);

  UniValue root(UniValue::VOBJ);
  root.push_back(Pair("snapshot_hash", snapshotHash.GetHex()));
  return root;
}

UniValue calcsnapshothash(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() < 2) {
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

    stream << std::vector<UTXO>(1);
    std::string inputs = HexStr(stream);
    stream.clear();

    stream << std::vector<UTXO>(1);
    std::string outputs = HexStr(stream);
    stream.clear();

    stream << uint256S("aa");
    std::string stakeModifier = HexStr(stream);
    stream.clear();

    SnapshotHash hash;
    stream << hash.GetData();
    std::string snapshotData = HexStr(stream);
    stream.clear();

    std::string example = inputs + " " + outputs + " " + stakeModifier + " " + snapshotData;
    throw std::runtime_error(
        "calcsnapshothash\n"
        "\nReturns snapshot hash and its data after arithmetic calculations\n"
        "\nArguments:\n"
        "1. \"inputs\" (hex, required) serialized UTXOs to subtract.\n"
        "2. \"outputs\" (hex, required) serialized UTXOs to add.\n"
        "3. \"stakeModifier\" (hex, required) stake modifier of the current block\n"
        "4. \"snapshotData\" (hex, optional) initial snapshot data.\n"
        "\nExamples:\n" +
        HelpExampleCli("calcsnapshothash", example) +
        HelpExampleRpc("calcsnapshothash", example));
  }

  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
  for (int i = 0; i < 2; ++i) {
    std::vector<uint8_t> data = ParseHex(request.params[i].get_str());
    stream.write(reinterpret_cast<char *>(data.data()), data.size());
  }

  std::vector<UTXO> inputs;
  std::vector<UTXO> outputs;
  stream >> inputs;
  stream >> outputs;

  uint256 stakeModifier = uint256(ParseHex(request.params[2].get_str()));

  SnapshotHash hash;
  if (request.params.size() == 4) {
    hash = SnapshotHash(ParseHex(request.params[3].get_str()));
  }

  for (const auto &in : inputs) {
    hash.SubtractUTXO(in);
  }

  for (const auto &out : outputs) {
    hash.AddUTXO(out);
  }

  UniValue root(UniValue::VOBJ);
  root.push_back(Pair("hash", HexStr(hash.GetHash(stakeModifier))));
  root.push_back(Pair("data", HexStr(hash.GetData())));
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
  root.push_back(Pair("hash", HexStr(hash.GetHash(chainActive.Tip()->bnStakeModifier))));
  root.push_back(Pair("data", HexStr(hash.GetData())));

  return root;
}

// clang-format off
static const CRPCCommand commands[] = {
    // category   name                actor (function)   argNames
    // --------   ------------------  -----------------  --------
    { "snapshot", "createsnapshot",   &createsnapshot,   {"maxutxosubsets"} },
    { "snapshot", "deletesnapshot",   &deletesnapshot,   {"snapshothash"} },
    { "snapshot", "getblocksnapshot", &getblocksnapshot, {"blockhash"} },
    { "snapshot", "listsnapshots",    &listsnapshots,    {""} },
    { "snapshot", "gettipsnapshot",   &gettipsnapshot,   {}},
    { "snapshot", "calcsnapshothash", &calcsnapshothash, {}},
};
// clang-format on

void RegisterRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
}  // namespace snapshot
