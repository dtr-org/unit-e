// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_custom_parameters.h>

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <blockchain/blockchain_types.h>
#include <util.h>

#include <univalue/include/univalue.h>
#include <boost/optional.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <type_traits>

namespace blockchain {

namespace {

enum class ReadResult {
  no_value_read,
  value_read_successfully,
  failed_to_read,
};

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    bool *value) {
  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isBool()) {
    return ReadResult::failed_to_read;
  }
  *value = json_value.get_bool();
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::int64_t *value) {
  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isNum()) {
    return ReadResult::failed_to_read;
  }
  try {
    *value = json_value.get_int64();
  } catch (const std::runtime_error &) {
    return ReadResult::failed_to_read;
  }
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::uint32_t *value) {

  std::int64_t intermediate_value;
  const ReadResult intermediate_result = Read(parameters, json_object, key, &intermediate_value);
  if (intermediate_result != ReadResult::value_read_successfully) {
    return intermediate_result;
  }
  if (intermediate_value < 0) {
    return ReadResult::failed_to_read;
  }
  if (intermediate_value > std::numeric_limits<std::uint32_t>::max()) {
    return ReadResult::failed_to_read;
  }
  *value = static_cast<std::uint32_t>(intermediate_value);
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::int32_t *value) {

  std::int64_t intermediate_value;
  const ReadResult intermediate_result = Read(parameters, json_object, key, &intermediate_value);
  if (intermediate_result != ReadResult::value_read_successfully) {
    return intermediate_result;
  }
  if (intermediate_value < std::numeric_limits<std::int32_t>::min()) {
    return ReadResult::failed_to_read;
  }
  if (intermediate_value > std::numeric_limits<std::int32_t>::max()) {
    return ReadResult::failed_to_read;
  }
  *value = static_cast<std::int32_t>(intermediate_value);
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::string *value) {
  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isStr()) {
    return ReadResult::failed_to_read;
  }
  *value = json_value.get_str();
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    uint256 *value) {

  std::string intermediate_value;
  const ReadResult intermediate_result = Read(parameters, json_object, key, &intermediate_value);
  if (intermediate_result != ReadResult::value_read_successfully) {
    return intermediate_result;
  }
  *value = uint256S(intermediate_value);
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    P2WPKH *value) {

  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isObject()) {
    return ReadResult::failed_to_read;
  }

  std::map<std::string, UniValue> obj;
  json_value.getObjMap(obj);

  if (Read(parameters, obj, "amount", &value->amount) != ReadResult::value_read_successfully) {
    return ReadResult::failed_to_read;
  }
  if (value->amount <= 0) {
    return ReadResult::failed_to_read;
  }
  if (Read(parameters, obj, "pub_key_hash", &value->pub_key_hash) != ReadResult::value_read_successfully) {
    return ReadResult::failed_to_read;
  }
  if (value->pub_key_hash.size() != 40) {
    return ReadResult::failed_to_read;
  }
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    P2WSH *value) {

  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isObject()) {
    return ReadResult::failed_to_read;
  }

  std::map<std::string, UniValue> obj;
  json_value.getObjMap(obj);

  if (Read(parameters, obj, "amount", &value->amount) != ReadResult::value_read_successfully) {
    return ReadResult::failed_to_read;
  }
  if (value->amount <= 0) {
    return ReadResult::failed_to_read;
  }
  if (Read(parameters, obj, "script_hash", &value->script_hash) != ReadResult::value_read_successfully) {
    return ReadResult::failed_to_read;
  }
  if (value->script_hash.size() != 64) {
    return ReadResult::failed_to_read;
  }
  return ReadResult::value_read_successfully;
}

template <typename T>
ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::vector<T> *value) {
  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isArray()) {
    return ReadResult::failed_to_read;
  }
  std::vector<T> result(json_value.size());
  for (std::size_t i = 0; i < json_value.size(); ++i) {
    std::map<std::string, UniValue> obj;
    obj[""] = json_value[i];
    if (Read(parameters, obj, "", &result[i]) == ReadResult::failed_to_read) {
      return ReadResult::failed_to_read;
    }
  }
  *value = std::move(result);
  return ReadResult::value_read_successfully;
}

ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    GenesisBlock *value) {
  if (json_object.count(key) == 0) {
    return ReadResult::no_value_read;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isObject()) {
    return ReadResult::failed_to_read;
  }
  GenesisBlockBuilder builder;

  std::map<std::string, UniValue> obj;
  json_value.getObjMap(obj);

  decltype(value->block.nVersion) version;
  switch (Read(parameters, obj, "version", &version)) {
    case ReadResult::value_read_successfully:
      builder.SetVersion(version);
      break;
    case ReadResult::no_value_read:
      break;
    case ReadResult::failed_to_read:
      return ReadResult::failed_to_read;
  }

  decltype(value->block.nTime) time;
  switch (Read(parameters, obj, "time", &time)) {
    case ReadResult::value_read_successfully:
      builder.SetTime(time);
      break;
    case ReadResult::no_value_read:
      break;
    case ReadResult::failed_to_read:
      return ReadResult::failed_to_read;
  }

  uint256 difficulty;
  switch (Read(parameters, obj, "difficulty", &difficulty)) {
    case ReadResult::value_read_successfully:
      builder.SetDifficulty(difficulty);
      break;
    case ReadResult::no_value_read:
      break;
    case ReadResult::failed_to_read:
      return ReadResult::failed_to_read;
  }

  std::vector<P2WPKH> p2wpkh_funds;
  switch (Read(parameters, obj, "p2wpkh_funds", &p2wpkh_funds)) {
    case ReadResult::value_read_successfully:
      for (const P2WPKH &p2wpkh : p2wpkh_funds) {
        builder.AddFundsForPayToPubKeyHash(p2wpkh.amount, p2wpkh.pub_key_hash);
      }
      break;
    case ReadResult::no_value_read:
      break;
    case ReadResult::failed_to_read:
      return ReadResult::failed_to_read;
  }

  std::vector<P2WSH> p2wsh_funds;
  switch (Read(parameters, obj, "p2wsh_funds", &p2wsh_funds)) {
    case ReadResult::value_read_successfully:
      for (const P2WSH &p2wsh : p2wsh_funds) {
        builder.AddFundsForPayToScriptHash(p2wsh.amount, p2wsh.script_hash);
      }
      break;
    case ReadResult::no_value_read:
      break;
    case ReadResult::failed_to_read:
      return ReadResult::failed_to_read;
  }

  *value = GenesisBlock(builder.Build(parameters));
  return ReadResult::value_read_successfully;
}

}  // namespace

blockchain::Parameters ReadCustomParametersFromJson(
    const UniValue &json,
    const blockchain::Parameters &base_parameters) {

  if (!json.isObject()) {
    throw FailedToParseCustomParametersError("Not a JSON object.");
  }
  std::map<std::string, UniValue> json_object;
  json.getObjMap(json_object);

  blockchain::Parameters parameters = base_parameters;
  std::vector<std::string> errors;

#define READ_PARAMETER(NAME)                                                                  \
  if (Read(parameters, json_object, #NAME, &parameters.NAME) == ReadResult::failed_to_read) { \
    errors.emplace_back("Failed to read \"" #NAME "\"");                                      \
  }
  // using the READ_PARAMETER macro ensures that no typos create a mismatch
  // between the json key (a string) and the parameters key (a member).
  READ_PARAMETER(network_name);
  READ_PARAMETER(block_stake_timestamp_interval_seconds);
  READ_PARAMETER(block_time_seconds);
  READ_PARAMETER(max_future_block_time_seconds);
  READ_PARAMETER(maximum_block_size);
  READ_PARAMETER(relay_non_standard_transactions);
  READ_PARAMETER(maximum_block_weight);
  READ_PARAMETER(maximum_block_serialized_size);
  READ_PARAMETER(maximum_sigops_count);
  READ_PARAMETER(coinbase_maturity);
  READ_PARAMETER(stake_maturity);
  READ_PARAMETER(stake_maturity_activation_height);
  READ_PARAMETER(initial_supply);
  READ_PARAMETER(reward);
  READ_PARAMETER(mine_blocks_on_demand);
  READ_PARAMETER(bech32_human_readable_prefix);
  READ_PARAMETER(deployment_confirmation_period);
  READ_PARAMETER(rule_change_activation_threshold);

  // load the genesis block last, as GenesisBlockBuilder::Build()
  // accepts the parameters read so far as an argument.
  READ_PARAMETER(genesis_block);

#undef READ_PARAMETER

  if (errors.size() > 0) {
    throw FailedToParseCustomParametersError(util::to_string(errors));
  }
  return parameters;
}

blockchain::Parameters ReadCustomParametersFromJsonString(
    const std::string &json_string,
    const blockchain::Parameters &base_parameters) {

  UniValue json;
  if (!json.read(json_string)) {
    throw FailedToParseCustomParametersError("Invalid JSON.");
  }
  return ReadCustomParametersFromJson(json, base_parameters);
}

blockchain::Parameters ReadCustomParametersFromFile(
    const std::string &filepath,
    const blockchain::Parameters &base_parameters) {
  std::ifstream t(filepath);
  std::stringstream buffer;
  buffer << t.rdbuf();
  return ReadCustomParametersFromJsonString(buffer.str(), base_parameters);
}

}  // namespace blockchain
