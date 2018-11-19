// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/rpc_processing.h>

#include <rpc/server.h>
#include <serialize.h>
#include <snapshot/creator.h>
#include <snapshot/indexer.h>
#include <snapshot/iterator.h>
#include <streams.h>
#include <univalue.h>
#include <utilstrencodings.h>
#include <validation.h>
#include <version.h>

namespace snapshot {

UniValue listsnapshots(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 0) {
    throw std::runtime_error(
        "listsnapshots\n"
        "\nList all snapshots.\n"
        "\nExamples:\n" +
        HelpExampleCli("listsnapshots", "") + HelpExampleRpc("listsnapshots", ""));
  }

  UniValue rootNode(UniValue::VOBJ);
  uint32_t snapshotId = 0;
  if (pcoinsdbview->GetSnapshotId(snapshotId)) {
    rootNode.push_back(Pair("snapshot_id", int(snapshotId)));
    std::unique_ptr<Indexer> idx = Indexer::Open(snapshotId);
    if (idx) {
      rootNode.push_back(Pair("snapshot_hash", idx->GetMeta().m_snapshotHash.GetHex()));
    }
  }

  if (pcoinsdbview->GetCandidateSnapshotId(snapshotId)) {
    rootNode.push_back(Pair("candidate_snapshot_id", int(snapshotId)));
    std::unique_ptr<Indexer> idx = Indexer::Open(snapshotId);
    if (idx) {
      rootNode.push_back(Pair("candidate_snapshot_hash", idx->GetMeta().m_snapshotHash.GetHex()));
    }
  }

  if (pcoinsdbview->GetInitSnapshotId(snapshotId)) {
    rootNode.push_back(Pair("init_snapshot_id", int(snapshotId)));
    std::unique_ptr<Indexer> idx = Indexer::Open(snapshotId);
    if (idx) {
      rootNode.push_back(Pair("init_snapshot_hash", idx->GetMeta().m_snapshotHash.GetHex()));
    }
  }

  return rootNode;
}

UniValue readsnapshot(const JSONRPCRequest &request) {
  if (request.fHelp || request.params.size() > 1) {
    throw std::runtime_error(
        "readsnapshot (<id>)\n"
        "\nReads the snapshot and prints its content.\n"
        "\nArguments:\n"
        "1. id (numeric, optional) Which snapshot to read. "
        "If id is missing, read the current one."
        "\nExamples:\n" +
        HelpExampleCli("readsnapshot", "0") + HelpExampleRpc("readsnapshot", "0"));
  }

  UniValue rootNode(UniValue::VOBJ);

  uint32_t snapshotId = 0;
  if (!request.params.empty()) {
    snapshotId = static_cast<uint32_t>(request.params[0].get_int());
  } else {
    if (!pcoinsdbview->GetSnapshotId(snapshotId)) {
      rootNode.push_back(Pair("error", "snapshot is missing"));
      return rootNode;
    }
  }

  std::unique_ptr<Indexer> idx = Indexer::Open(snapshotId);
  if (idx == nullptr) {
    rootNode.push_back(Pair("error", "can't read snapshot"));
    return rootNode;
  }

  Iterator iter(std::move(idx));
  int totalOutputs = 0;
  while (iter.Valid()) {
    UTXOSubset subset = iter.GetUTXOSubset();
    totalOutputs += subset.m_outputs.size();

    iter.Next();
  }

  rootNode.push_back(Pair("snapshot_hash", iter.GetSnapshotHash().GetHex()));
  rootNode.push_back(Pair("snapshot_id", int(iter.GetSnapshotId())));
  rootNode.push_back(Pair("best_block_hash", iter.GetBestBlockHash().GetHex()));
  rootNode.push_back(Pair("total_utxo_subsets", iter.GetTotalUTXOSubsets()));
  rootNode.push_back(Pair("total_outputs", totalOutputs));

  {
    uint32_t id = 0;
    pcoinsdbview->GetSnapshotId(id);
    rootNode.push_back(Pair("current_snapshot_id", static_cast<uint64_t>(id)));
  }

  UniValue ids(UniValue::VARR);
  for (uint32_t &id : pcoinsdbview->GetSnapshotIds()) {
    ids.push_back(static_cast<uint64_t>(id));
  }
  rootNode.push_back(Pair("all_snapshot_ids", ids));

  return rootNode;
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

  UniValue rootNode(UniValue::VOBJ);
  CreationInfo info = creator.Create();
  if (info.m_status != +Status::OK) {
    switch (info.m_status) {
      case Status::WRITE_ERROR:
        rootNode.push_back(Pair("error", "can't write to any *.dat files"));
        break;
      case Status::RESERVE_SNAPSHOT_ID_ERROR:
        rootNode.push_back(Pair("error", "can't reserve snapshot ID"));
        break;
      case Status::SET_SNAPSHOT_ID_ERROR:
        rootNode.push_back(Pair("error", "can't set new snapshot ID"));
        break;
      case Status::CALC_SNAPSHOT_HASH_ERROR:
        rootNode.push_back(Pair("error", "can't calculate hash of the snapshot"));
        break;
      default:
        rootNode.push_back(Pair("error", "unknown error happened during creating snapshot"));
    }
    return rootNode;
  }

  rootNode.push_back(Pair("snapshot_hash", info.m_indexerMeta.m_snapshotHash.GetHex()));
  rootNode.push_back(Pair("best_block_hash", info.m_indexerMeta.m_bestBlockHash.GetHex()));
  rootNode.push_back(Pair("total_utxo_subsets", info.m_indexerMeta.m_totalUTXOSubsets));
  rootNode.push_back(Pair("total_outputs", info.m_totalOutputs));

  uint32_t snapshotId = 0;
  pcoinsdbview->GetSnapshotId(snapshotId);
  rootNode.push_back(Pair("current_snapshot_id", static_cast<uint64_t>(snapshotId)));

  UniValue ids(UniValue::VARR);
  for (uint32_t &id : pcoinsdbview->GetSnapshotIds()) {
    ids.push_back(static_cast<uint64_t>(id));
  }
  rootNode.push_back(Pair("all_snapshot_ids", ids));

  return rootNode;
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
    // category     name                 actor (function)   argNames
    // --------     -----------------   ----------------   --------
    { "blockchain", "createsnapshot",   &createsnapshot,   {"maxutxosubsets"} },
    { "blockchain", "readsnapshot",     &readsnapshot,     {"id"} },
    { "blockchain", "listsnapshots",    &listsnapshots,    {""} },
    { "blockchain", "gettipsnapshot",   &gettipsnapshot,   {}},
    { "blockchain", "calcsnapshothash", &calcsnapshothash, {}},
};
// clang-format on

void RegisterRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
}  // namespace snapshot
