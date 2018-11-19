// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationparams.h>

#include <univalue/include/univalue.h>
#include <util.h>

namespace esperanza {

int64_t ParseNum(const UniValue &value, std::string key, int64_t defVal) {

  try {
    if (!value[key].isNull() && value[key].isNum()) {
      return value[key].get_int64();
    }
  } catch (std::exception &e) {
    fprintf(stderr, "Error: Cannot parse parameter %s as numeric value!\n",
            key.c_str());
  }

  return defVal;
}

bool ParseFinalizationParams(std::string jsonString,
                             FinalizationParams &paramsOut) {

  UniValue json;
  if (!json.read(jsonString)) {
    LogPrintf("Malformed json object: %s\n", jsonString);
    return false;
  }

  paramsOut.m_epochLength = ParseNum(json, "epochLength", 50);
  paramsOut.m_minDepositSize = ParseNum(json, "minDepositSize", 1500 * UNIT);
  paramsOut.m_dynastyLogoutDelay = ParseNum(json, "dynastyLogoutDelay", 700);
  paramsOut.m_withdrawalEpochDelay =
      ParseNum(json, "withdrawalEpochDelay", static_cast<int>(1.5e4));

  paramsOut.m_slashFractionMultiplier =
      ParseNum(json, "slashFractionMultiplier", 3);
  paramsOut.m_bountyFractionDenominator =
      ParseNum(json, "bountyFractionDenominator", 25);

  int64_t baseInterestFactor = ParseNum(json, "baseInterestFactor", 7);
  if (baseInterestFactor < 0) {
    LogPrintf("Param baseInterestFactor must be a positive number.\n");
    return false;
  }
  paramsOut.m_baseInterestFactor =
      ufp64::to_ufp64((uint64_t)baseInterestFactor);

  int64_t basePenaltyFactor = ParseNum(json, "basePenaltyFactor", 2);
  if (basePenaltyFactor < 0) {
    LogPrintf("Param basePenaltyFactor must be a positive number.\n");
    return false;
  }
  paramsOut.m_basePenaltyFactor =
      ufp64::div_2uint((uint64_t)basePenaltyFactor, 10000000);
  return true;
}

}  // namespace esperanza
