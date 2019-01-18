// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/finalizationparams.h>

#include <univalue/include/univalue.h>
#include <util.h>

namespace esperanza {

int64_t ParseNum(const UniValue &value, const std::string &key, int64_t def_val) {

  try {
    const UniValue &val = value[key];
    if (!val.isNull() && val.isNum()) {
      return val.get_int64();
    }
  } catch (std::exception &e) {
    fprintf(stderr, "Error: Cannot parse parameter %s as numeric value!\n",
            key.c_str());
  }

  return def_val;
}

bool ParseFinalizationParams(const std::string &json_string,
                             FinalizationParams &params_out) {

  UniValue json;
  if (!json.read(json_string)) {
    LogPrintf("Malformed json object: %s\n", json_string);
    return false;
  }

  params_out.epoch_length = static_cast<uint32_t>(ParseNum(json, "epochLength", params_out.epoch_length));

  params_out.min_deposit_size = ParseNum(json, "minDepositSize", params_out.min_deposit_size);

  params_out.dynasty_logout_delay = ParseNum(json, "dynastyLogoutDelay", params_out.dynasty_logout_delay);

  params_out.withdrawal_epoch_delay = ParseNum(json, "withdrawalEpochDelay", params_out.withdrawal_epoch_delay);

  params_out.slash_fraction_multiplier = ParseNum(json, "slashFractionMultiplier", params_out.slash_fraction_multiplier);

  params_out.bounty_fraction_denominator = ParseNum(json, "bountyFractionDenominator", params_out.bounty_fraction_denominator);

  auto value = ParseNum(json, "baseInterestFactor", params_out.base_interest_factor);
  if (value < 0) {
    LogPrintf("Param baseInterestFactor must be a positive number.\n");
    return false;
  }
  params_out.base_interest_factor = ufp64::to_ufp64((uint64_t)value);

  value = ParseNum(json, "basePenaltyFactor", ufp64::mul_to_uint(params_out.base_penalty_factor, 10000000));
  if (value < 0) {
    LogPrintf("Param basePenaltyFactor must be a positive number.\n");
    return false;
  }
  params_out.base_penalty_factor = ufp64::div_by_uint(ufp64::to_ufp64((uint64_t)value), 10000000);

  return true;
}

FinalizationParams::FinalizationParams() : epoch_length{5},
                                           min_deposit_size{1500 * UNIT},
                                           dynasty_logout_delay{2},
                                           withdrawal_epoch_delay{5},
                                           slash_fraction_multiplier{3},
                                           bounty_fraction_denominator{25},
                                           base_interest_factor{ufp64::to_ufp64(700)},
                                           base_penalty_factor{ufp64::div_2uint(2, 10000000)} {}
}  // namespace esperanza
