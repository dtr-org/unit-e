// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <esperanza/finalizationstate.h>
#include <injector.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
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

  ObserveSafeMode();

  LOCK(GetComponent<finalization::StateRepository>()->GetLock());
  const finalization::FinalizationState *fin_state =
      GetComponent<finalization::StateRepository>()->GetTipState();
  assert(fin_state != nullptr);

  UniValue obj(UniValue::VOBJ);

  obj.pushKV("currentDynasty", (uint64_t)fin_state->GetCurrentDynasty());
  obj.pushKV("currentEpoch", (uint64_t)fin_state->GetCurrentEpoch());
  obj.pushKV("lastJustifiedEpoch", (uint64_t)fin_state->GetLastJustifiedEpoch());
  obj.pushKV("lastFinalizedEpoch", (uint64_t)fin_state->GetLastFinalizedEpoch());
  obj.pushKV("validators", (uint64_t)fin_state->GetActiveFinalizers().size());

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

  ObserveSafeMode();

  const esperanza::FinalizationParams params = Params().GetFinalization();
  UniValue obj(UniValue::VOBJ);

  obj.pushKV("epochLength", (uint64_t)params.epoch_length);
  obj.pushKV("minDepositSize", (uint64_t)params.min_deposit_size);
  obj.pushKV("dynastyLogoutDelay", (uint64_t)params.dynasty_logout_delay);
  obj.pushKV("withdrawalEpochDelay", (uint64_t)params.withdrawal_epoch_delay);
  obj.pushKV("bountyFractionDenominator", (uint64_t)params.bounty_fraction_denominator);
  obj.pushKV("slashFractionMultiplier", (uint64_t)params.slash_fraction_multiplier);
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
