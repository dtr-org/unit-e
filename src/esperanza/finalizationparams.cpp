#include "finalizationparams.h"
#include <esperanza/finalizationparams.h>
#include <univalue/include/univalue.h>

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
    return false;
  }
  paramsOut.m_baseInterestFactor =
      ufp64::to_ufp64((uint64_t)baseInterestFactor);

  int64_t basePenaltyFactor = ParseNum(json, "basePenaltyFactor", 2);
  if (basePenaltyFactor < 0) {
    return false;
  }
  paramsOut.m_basePenaltyFactor =
      ufp64::div_2uint((uint64_t)basePenaltyFactor, 10000000);
  return true;
}

}  // namespace esperanza
