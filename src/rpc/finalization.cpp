// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <injector.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <ufp64.h>
#include <util.h>
#include <utilstrencodings.h>

UniValue getfinalizationstate(const JSONRPCRequest &request) {
  if (request.fHelp || !request.params.empty()) {
    throw std::runtime_error(
        "getfinalizationstate\n"
        "Returns an object containing finalization information."
        "\nResult:\n"
        "{\n"
        "  \"currentDynasty\": xxxxxxx          (numeric) currentDynasty\n"
        "  \"currentDynastyStartsAtEpoch\": xxxxxxx    (numeric) epoch of the current dynasty\n"
        "  \"currentEpoch\": xxxxxxx            (numeric) currentEpoch\n"
        "  \"lastJustifiedEpoch\": xxxxxxx      (numeric) lastJustifiedEpoch\n"
        "  \"lastFinalizedEpoch\": xxxxxxx      (numeric) lastFinalizedEpoch\n"
        "  \"validators\": xxxxxxx              (numeric) current number of "
        "active validators\n"
        "}\n"
        "\nExamples:\n" +
        HelpExampleCli("getfinalizationstate", "") +
        HelpExampleRpc("getfinalizationstate", ""));
  }

  LOCK(GetComponent<finalization::StateRepository>()->GetLock());
  const finalization::FinalizationState *fin_state =
      GetComponent<finalization::StateRepository>()->GetTipState();
  assert(fin_state != nullptr);

  UniValue obj(UniValue::VOBJ);

  obj.pushKV("currentDynasty", ToUniValue(fin_state->GetCurrentDynasty()));
  obj.pushKV("currentDynastyStartsAtEpoch", ToUniValue(fin_state->GetCurrentDynastyEpochStart()));
  obj.pushKV("currentEpoch", ToUniValue(fin_state->GetCurrentEpoch()));
  obj.pushKV("lastJustifiedEpoch", ToUniValue(fin_state->GetLastJustifiedEpoch()));
  obj.pushKV("lastFinalizedEpoch", ToUniValue(fin_state->GetLastFinalizedEpoch()));
  obj.pushKV("validators", static_cast<std::uint64_t>(fin_state->GetActiveFinalizers().size()));

  return obj;
}

UniValue getfinalizationconfig(const JSONRPCRequest &request) {
  if (request.fHelp || !request.params.empty()) {
    throw std::runtime_error(
        "getfinalizationconfig\n"
        "Returns an object containing the esperanza protocol configuration."
        "\nResult:\n"
        "{\n"
        "  \"epochLength\": xxxxxxx        (numeric) size of the epoch expressed in blocks\n"
        "  \"minDepositSize\": xxxxxxx        (numeric) minimum deposit size allowed to become validator\n"
        "  \"dynastyLogoutDelay\": xxxxxxx        (numeric) minimum delay in dynasties before a logout can be performed\n"
        "  \"withdrawalEpochDelay\": xxxxxxx        (numeric) minimum delay in epochs before a withdrawal can take place\n"
        "  \"bountyFractionDenominator\": xxxxxxx        (numeric) the bounty reward for reporting a slashable behaviour is defined by 1/x\n"
        "  \"slashFractionMultiplier\": xxxxxxx        (numeric) multiplier for slashing the deposit of a misbehaving validator\n"
        "  \"baseInterestFactor\": xxxxxxx        (numeric) base interest factor\n"
        "  \"basePenaltyFactor\": xxxxxxx        (numeric) base penalty factor\n"
        "}\n"
        "\nExamples:\n" +
        HelpExampleRpc("getfinalizationconfig", ""));
  }

  const finalization::Params &params = *GetComponent<finalization::Params>();
  UniValue obj(UniValue::VOBJ);

  obj.pushKV("epochLength", ToUniValue(params.epoch_length));
  obj.pushKV("minDepositSize", ToUniValue(params.min_deposit_size));
  obj.pushKV("dynastyLogoutDelay", ToUniValue(params.dynasty_logout_delay));
  obj.pushKV("withdrawalEpochDelay", ToUniValue(params.withdrawal_epoch_delay));
  obj.pushKV("bountyFractionDenominator", ToUniValue(params.bounty_fraction_denominator));
  obj.pushKV("slashFractionMultiplier", ToUniValue(params.slash_fraction_multiplier));
  obj.pushKV("baseInterestFactor", ufp64::to_str(params.base_interest_factor));
  obj.pushKV("basePenaltyFactor", ufp64::to_str(params.base_penalty_factor));

  return obj;
}

// clang-format off
static const CRPCCommand commands[] =
{ //  category        name                      actor (function)            argNames
  //  --------        -------------------       ----------------            ----------
    { "finalization",  "getfinalizationstate",   &getfinalizationstate,       {}          },
    { "finalization",  "getfinalizationconfig",  &getfinalizationconfig,      {}          },
};
// clang-format on

void RegisterFinalizationRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
