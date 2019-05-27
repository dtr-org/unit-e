// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <finalization/params.h>

#include <univalue/include/univalue.h>

namespace finalization {

namespace {

esperanza::AdminKeySet CreateAdminKeys(std::array<std::string, esperanza::ADMIN_MULTISIG_KEYS> &&pubkeys) {
  esperanza::AdminKeySet key_set;
  for (int i = 0; i < pubkeys.size(); ++i) {
    std::vector<unsigned char> key_data = ParseHex(std::move(pubkeys[i]));
    CPubKey key(key_data);
    assert(key.IsValid());
    key_set[i] = std::move(key);
  }
  return key_set;
}

int64_t ParseNum(const UniValue &value, const std::string &key, int64_t def_val) {
  const UniValue &val = value[key];
  if (!val.isNull() && val.isNum()) {
    return val.get_int64();
  }
  return def_val;
}

Params FromJson(const std::string &json_string, finalization::Params p) {
  UniValue json;
  if (!json.read(json_string)) {
    throw std::runtime_error("Malformed finalization config: " + json_string);
  }

  p.epoch_length = static_cast<uint32_t>(ParseNum(json, "epochLength", p.epoch_length));
  p.min_deposit_size = ParseNum(json, "minDepositSize", p.min_deposit_size);
  p.dynasty_logout_delay = ParseNum(json, "dynastyLogoutDelay", p.dynasty_logout_delay);
  p.withdrawal_epoch_delay = ParseNum(json, "withdrawalEpochDelay", p.withdrawal_epoch_delay);
  p.slash_fraction_multiplier = ParseNum(json, "slashFractionMultiplier", p.slash_fraction_multiplier);
  p.bounty_fraction_denominator = ParseNum(json, "bountyFractionDenominator", p.bounty_fraction_denominator);

  {
    const auto value = ParseNum(json, "baseInterestFactor", p.base_interest_factor);
    if (value < 0) {
      throw std::runtime_error("Param baseInterestFactor must be a positive number");
    }
    p.base_interest_factor = ufp64::ufp64_t(value);
  }

  {
    const auto value = ParseNum(json, "basePenaltyFactor", p.base_penalty_factor);
    if (value < 0) {
      throw std::runtime_error("Param basePenaltyFactor must be a positive number");
    }
    p.base_penalty_factor = ufp64::ufp64_t(value);
  }

  return p;
}
}  // namespace

Params Params::RegTest(const bool gen_admin_keys) {
  Params p;
  p.epoch_length = 5;
  p.min_deposit_size = 1500 * UNIT;
  p.dynasty_logout_delay = 2;
  p.withdrawal_epoch_delay = 5;
  p.slash_fraction_multiplier = 3;
  p.bounty_fraction_denominator = 25;
  p.base_interest_factor = ufp64::to_ufp64(7);
  p.base_penalty_factor = ufp64::div_2uint(2, 100000);
  if (gen_admin_keys) {
    p.admin_params.admin_keys = CreateAdminKeys(
        {"038c0246da82d686e4638d8cf60452956518f8b63c020d23387df93d199fc089e8",
         "02f1563a8930739b653426380a8297e5f08682cb1e7c881209aa624f821e2684fa",
         "03d2bc85e0b035285add07680695cb561c9b9fbe9cb3a4be4f1f5be2fc1255944c"});
  }
  return p;
}

Params Params::TestNet(const bool gen_admin_keys) {
  Params p;
  p.epoch_length = 50;
  p.min_deposit_size = 10000 * UNIT;
  p.dynasty_logout_delay = 5;
  p.withdrawal_epoch_delay = 10;
  p.slash_fraction_multiplier = 3;
  p.bounty_fraction_denominator = 25;
  p.base_interest_factor = ufp64::to_ufp64(7);
  p.base_penalty_factor = ufp64::div_2uint(2, 10000000);
  if (gen_admin_keys) {
    p.admin_params.admin_keys = CreateAdminKeys(
        {"02630a75cd35adc6c44ca677e83feb8e4a7e539baaa49887c455e8242e3e3b1c05",
         "03946025d10e3cdb30a9cd73525bc9acc4bd92e184cdd9c9ea7d0ebc6b654afcc5",
         "0290f45494a197cbd389181b3d7596a90499a93368159e8a6e9f9d0d460799d33d"});
  }
  return p;
}

std::unique_ptr<Params> Params::New(Dependency<UnitEInjectorConfiguration> cfg,
                                    Dependency<ArgsManager> args) {
  Params p;
  if (args->GetBoolArg("-regtest", false)) {
    p = RegTest(args->GetBoolArg("-permissioning", false));
  } else {
    p = TestNet(true);
  }

  if (args->IsArgSet("-esperanzaconfig")) {
    p = FromJson(args->GetArg("-esperanzaconfig", "{}"), p);
  }

  if (cfg->disable_finalization) {
    p.epoch_length = 9999999;
  }

  return MakeUnique<Params>(p);
}

}  // namespace finalization
