#include <chainparams.h>
#include <esperanza/params.h>
#include <esperanza/finalizationstate.h>
#include <rpc/safemode.h>
#include <rpc/server.h>
#include <ufp64.h>
#include <util.h>
#include <utilstrencodings.h>

UniValue getfinalizationstate(const JSONRPCRequest &request) {
  if (request.fHelp || !request.params.empty()) {
    throw std::runtime_error(
        "getesperanzastate\n"
        "Returns an object containing finalization information."
        "\nResult:\n"
        "{\n"
        "  \"currentEpoch\": xxxxxxx            (numeric) currentEpoch\n"
        "  \"currentDynasty\": xxxxxxx          (numeric) currentDynasty\n"
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

  esperanza::FinalizationState &finalizationState = *esperanza::FinalizationState::GetState();
  UniValue obj(UniValue::VOBJ);

  obj.pushKV("currentEpoch", (uint64_t) finalizationState.GetCurrentEpoch());
  obj.pushKV("currentDynasty", (uint64_t) finalizationState.GetCurrentDynasty());
  obj.pushKV("lastFinalizedEpoch", (uint64_t) finalizationState.GetLastFinalizedEpoch());
  obj.pushKV("lastJustifiedEpoch", (uint64_t) finalizationState.GetLastJustifiedEpoch());
  obj.pushKV("validators", (uint64_t) finalizationState.GetValidators().size());

  return obj;
}

UniValue getesperanzaconfig(const JSONRPCRequest &request) {
  if (request.fHelp || !request.params.empty()) {
    throw std::runtime_error(
        "getesperanzaconfig\n"
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
        HelpExampleRpc("getesperanzaconfig", ""));
  }

  ObserveSafeMode();

  const esperanza::FinalizationParams params = Params().GetFinalization();
  UniValue obj(UniValue::VOBJ);

  obj.pushKV("epochLength", (uint64_t) params.m_epochLength);
  obj.pushKV("minDepositSize", (uint64_t) params.m_minDepositSize);
  obj.pushKV("dynastyLogoutDelay", (uint64_t) params.m_dynastyLogoutDelay);
  obj.pushKV("withdrawalEpochDelay", (uint64_t) params.m_withdrawalEpochDelay);
  obj.pushKV("bountyFractionDenominator", (uint64_t) params.m_bountyFractionDenominator);
  obj.pushKV("slashFractionMultiplier", (uint64_t) params.m_slashFractionMultiplier);
  obj.pushKV("baseInterestFactor", ufp64::to_str(params.m_baseInterestFactor));
  obj.pushKV("basePenaltyFactor", ufp64::to_str(params.m_basePenaltyFactor));

  return obj;
}

// clang-format off
static const CRPCCommand commands[] =
        {   //  category    name                    actor (function)            argNames
            //  --------    -------------------     ----------------            ----------
            { "esperanza",  "getesperanzastate",    &getfinalizationstate,      {}          },
            { "esperanza",  "getesperanzaconfig",   &getesperanzaconfig,        {}          },
        };
// clang-format on

void RegisterEsperanzaRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
