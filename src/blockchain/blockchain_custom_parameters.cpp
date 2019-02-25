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
  NO_VALUE_READ,
  VALUE_READ_SUCCESSFULLY,
  FAILED_TO_READ,
};

template <typename T>
ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    T &value) {
  static_assert(std::is_void<T>::value, "unsupported type to Read()");
  return ReadResult::FAILED_TO_READ;
}

template <>
ReadResult Read<bool>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    bool &value) {
  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isBool()) {
    return ReadResult::FAILED_TO_READ;
  }
  value = json_value.get_bool();
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template <>
ReadResult Read<std::int64_t>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::int64_t &value) {
  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isNum()) {
    return ReadResult::FAILED_TO_READ;
  }
  try {
    value = json_value.get_int64();
  } catch (const std::runtime_error &) {
    return ReadResult::FAILED_TO_READ;
  }
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template <>
ReadResult Read<std::uint32_t>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::uint32_t &value) {

  std::int64_t intermediate_value;
  const ReadResult intermediate_result = Read(parameters, json_object, key, intermediate_value);
  if (intermediate_result != ReadResult::VALUE_READ_SUCCESSFULLY) {
    return intermediate_result;
  }
  if (intermediate_value < 0) {
    return ReadResult::FAILED_TO_READ;
  }
  if (intermediate_value > std::numeric_limits<std::uint32_t>::max()) {
    return ReadResult::FAILED_TO_READ;
  }
  value = static_cast<std::uint32_t>(intermediate_value);
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template <>
ReadResult Read<std::string>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::string &value) {
  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isStr()) {
    return ReadResult::FAILED_TO_READ;
  }
  value = json_value.get_str();
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template <>
ReadResult Read<uint256>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    uint256 &value) {

  std::string intermediate_value;
  const ReadResult intermediate_result = Read(parameters, json_object, key, intermediate_value);
  if (intermediate_result != ReadResult::VALUE_READ_SUCCESSFULLY) {
    return intermediate_result;
  }
  value = uint256S(intermediate_value);
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template<>
ReadResult Read<P2WPKH>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    P2WPKH &value) {

  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isObject()) {
    return ReadResult::FAILED_TO_READ;
  }

  std::map<std::string, UniValue> obj;
  json_value.getObjMap(obj);

  if (Read(parameters, obj, "amount", value.amount) != ReadResult::VALUE_READ_SUCCESSFULLY) {
    return ReadResult::FAILED_TO_READ;
  }
  std::string key_hash;
  if (Read(parameters, obj, "pub_key_hash", value.pub_key_hash) != ReadResult::VALUE_READ_SUCCESSFULLY) {
    return ReadResult::FAILED_TO_READ;
  }
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template<>
ReadResult Read<P2WSH>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    P2WSH &value) {

  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isObject()) {
    return ReadResult::FAILED_TO_READ;
  }

  std::map<std::string, UniValue> obj;
  json_value.getObjMap(obj);

  if (Read(parameters, obj, "amount", value.amount) != ReadResult::VALUE_READ_SUCCESSFULLY) {
    return ReadResult::FAILED_TO_READ;
  }
  std::string key_hash;
  if (Read(parameters, obj, "script_hash", value.script_hash) != ReadResult::VALUE_READ_SUCCESSFULLY) {
    return ReadResult::FAILED_TO_READ;
  }
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template <typename T>
ReadResult Read(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    std::vector<T> &value) {
  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isArray()) {
    return ReadResult::FAILED_TO_READ;
  }
  std::vector<T> result(json_value.size());
  for (std::size_t i = 0; i < json_value.size(); ++i) {
    std::map<std::string, UniValue> obj;
    obj[""] = json_value[i];
    if (Read(parameters, obj, "", result[i]) == ReadResult::FAILED_TO_READ) {
      return ReadResult::FAILED_TO_READ;
    }
  }
  value = result;
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

template <>
ReadResult Read<GenesisBlock>(
    const blockchain::Parameters &parameters,
    const std::map<std::string, UniValue> &json_object,
    const char *const key,
    GenesisBlock &value) {
  if (json_object.count(key) == 0) {
    return ReadResult::NO_VALUE_READ;
  }
  const UniValue &json_value = json_object.at(key);
  if (!json_value.isObject()) {
    return ReadResult::FAILED_TO_READ;
  }
  GenesisBlockBuilder builder;

  std::map<std::string, UniValue> obj;
  json_value.getObjMap(obj);

  uint256 difficulty;
  switch (Read(parameters, obj, "difficulty", difficulty)) {
    case ReadResult::VALUE_READ_SUCCESSFULLY:
      builder.SetDifficulty(difficulty);
    case ReadResult::NO_VALUE_READ:
      break;
    case ReadResult::FAILED_TO_READ:
      return ReadResult::FAILED_TO_READ;
  }

  blockchain::Time time;
  switch (Read(parameters, obj, "time", time)) {
    case ReadResult::VALUE_READ_SUCCESSFULLY:
      builder.SetTime(time);
    case ReadResult::NO_VALUE_READ:
      break;
    case ReadResult::FAILED_TO_READ:
      return ReadResult::FAILED_TO_READ;
  }

  std::uint32_t version;
  switch (Read(parameters, obj, "version", version)) {
    case ReadResult::VALUE_READ_SUCCESSFULLY:
      builder.SetVersion(version);
    case ReadResult::NO_VALUE_READ:
      break;
    case ReadResult::FAILED_TO_READ:
      return ReadResult::FAILED_TO_READ;
  }

  std::vector<P2WPKH> p2wpkh_funds;
  switch (Read(parameters, obj, "p2wpkh_funds", p2wpkh_funds)) {
    case ReadResult::VALUE_READ_SUCCESSFULLY:
      for (const P2WPKH &funds : p2wpkh_funds) {
        builder.AddFundsForPayToPubKeyHash(funds.amount, funds.pub_key_hash);
      }
    case ReadResult::NO_VALUE_READ:
      break;
    case ReadResult::FAILED_TO_READ:
      return ReadResult::FAILED_TO_READ;
  }

  std::vector<P2WSH> p2wsh_funds;
  switch (Read(parameters, obj, "p2wsh_funds", p2wsh_funds)) {
    case ReadResult::VALUE_READ_SUCCESSFULLY:
      for (const P2WSH &funds : p2wsh_funds) {
        builder.AddFundsForPayToScriptHash(funds.amount, funds.script_hash);
      }
    case ReadResult::NO_VALUE_READ:
      break;
    case ReadResult::FAILED_TO_READ:
      return ReadResult::FAILED_TO_READ;
  }

  value = GenesisBlock(builder.Build(parameters));
  return ReadResult::VALUE_READ_SUCCESSFULLY;
}

}  // namespace

boost::optional<blockchain::Parameters> ReadCustomParametersFromJson(
    const UniValue &json,
    const blockchain::Parameters &base_parameters,
    const std::function<void(const std::string &)> &report_error) {

  if (!json.isObject()) {
    return boost::none;
  }
  std::map<std::string, UniValue> json_object;
  json.getObjMap(json_object);

  std::size_t error_count = 0;
  blockchain::Parameters parameters = base_parameters;

#define READ_PARAMETER(NAME)                                                                 \
  if (Read(parameters, json_object, #NAME, parameters.NAME) == ReadResult::FAILED_TO_READ) { \
    ++error_count;                                                                           \
    report_error("Failed to read \"" #NAME "\"");                                            \
  }

  // using the READ_PARAMETER macro ensures that no typos create a mismatch
  // between the json key and the parameters key.

  READ_PARAMETER(network_name);
  READ_PARAMETER(block_stake_timestamp_interval_seconds);
  READ_PARAMETER(block_time_seconds);
  READ_PARAMETER(max_future_block_time_seconds);
  READ_PARAMETER(maximum_block_size);
  READ_PARAMETER(relay_non_standard_transactions);
  READ_PARAMETER(maximum_block_weight);
  READ_PARAMETER(maximum_block_serialized_size);
  READ_PARAMETER(maximum_block_sigops_cost);
  READ_PARAMETER(coinbase_maturity);
  READ_PARAMETER(stake_maturity);
  READ_PARAMETER(initial_supply);
  READ_PARAMETER(maximum_supply);
  READ_PARAMETER(reward_schedule);
  READ_PARAMETER(period_blocks);
  READ_PARAMETER(mine_blocks_on_demand);
  READ_PARAMETER(bech32_human_readable_prefix);
  READ_PARAMETER(deployment_confirmation_period);
  READ_PARAMETER(rule_change_activation_threshold);
  READ_PARAMETER(genesis_block);

#undef READ_PARAMETER

  if (error_count > 0) {
    return boost::none;
  }

  return parameters;
}

boost::optional<blockchain::Parameters> ReadCustomParametersFromJson(
    const UniValue &json,
    const blockchain::Parameters &base_parameters) {

  return ReadCustomParametersFromJson(json, base_parameters, [](const std::string &ignore) {});
}

boost::optional<blockchain::Parameters> ReadCustomParametersFromJsonString(
    const std::string &json_string,
    const blockchain::Parameters &base_parameters,
    const std::function<void(const std::string &)> &report_error) {

  UniValue json;
  if (!json.read(json_string)) {
    return boost::none;
  }
  return ReadCustomParametersFromJson(json, base_parameters, report_error);
}

boost::optional<blockchain::Parameters> ReadCustomParametersFromJsonString(
    const std::string &json_string,
    const blockchain::Parameters &base_parameters) {

  return ReadCustomParametersFromJsonString(json_string, base_parameters, [](const std::string &ignore) {});
}

}  // namespace blockchain
