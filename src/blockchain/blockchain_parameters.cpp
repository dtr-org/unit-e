// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>

#include <blockchain/blockchain_genesis.h>
#include <utilstrencodings.h>

namespace blockchain {

namespace {

Parameters BuildMainNetParameters() {
  Parameters p{};  // designated initializers would be so nice here
  p.networkName = "main";
  p.blockStakeTimestampIntervalSeconds = 16;
  p.blockTimeSeconds = 16;
  p.relayNonStandardTransactions = false;
  p.requireStandard = true;
  p.mineBlocksOnDemand = false;
  p.maximumBlockSerializedSize = 4000000;
  p.maximumBlockWeight = 4000000;
  p.maximumBlockSigopsCost = 80000;
  p.coinstakeMaturity = 100;
  p.rewardFunction = [](const Parameters &p, MoneySupply s, BlockHeight h) -> CAmount {
    constexpr uint64_t secondsInAYear = 365 * 24 * 60 * 60;
    // 2 percent inflation (2% of current money supply distributed over all blocks in a year)
    return (s * 2 / 100) / (secondsInAYear / p.blockStakeTimestampIntervalSeconds);
  };
  p.messageStartChars[0] = 0xee;
  p.messageStartChars[1] = 0xee;
  p.messageStartChars[2] = 0xae;
  p.messageStartChars[3] = 0xc1;
  p.deploymentConfirmationPeriod = 2016;
  p.ruleChangeActivationThreshold = 1916;

  static GenesisBlock genesisBlock{
      GenesisOutput(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
      GenesisOutput(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
      GenesisOutput(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
      GenesisOutput(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
      GenesisOutput(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867")};
  p.genesisBlock = &genesisBlock;

  return p;
}

Parameters BuildTestNetParameters() {
  Parameters p = Parameters::MainNet();
  p.networkName = "test";
  p.relayNonStandardTransactions = true;
  p.requireStandard = false;
  p.messageStartChars[0] = 0xfd;
  p.messageStartChars[1] = 0xfc;
  p.messageStartChars[2] = 0xfb;
  p.messageStartChars[3] = 0xfa;

  static GenesisBlock genesisBlock{
      GenesisOutput(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
      GenesisOutput(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
      GenesisOutput(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
      GenesisOutput(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
      GenesisOutput(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867")};
  p.genesisBlock = &genesisBlock;

  return p;
}

Parameters BuildRegTestParameters() {
  Parameters p = Parameters::MainNet();
  p.networkName = "regtest";
  p.mineBlocksOnDemand = true;
  p.coinstakeMaturity = 2;
  p.messageStartChars[0] = 0xfa;
  p.messageStartChars[1] = 0xbf;
  p.messageStartChars[2] = 0xb5;
  p.messageStartChars[3] = 0xda;

  static GenesisBlock genesisBlock{
      GenesisOutput(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
      GenesisOutput(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
      GenesisOutput(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
      GenesisOutput(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
      GenesisOutput(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867")};
  p.genesisBlock = &genesisBlock;

  return p;
}

}  // namespace

const Parameters &Parameters::MainNet() {
  static Parameters parameters = BuildMainNetParameters();
  return parameters;
}

const Parameters &Parameters::TestNet() {
  static Parameters parameters = BuildTestNetParameters();
  return parameters;
}

const Parameters &Parameters::RegTest() {
  static Parameters parameters = BuildRegTestParameters();
  return parameters;
}

}  // namespace blockchain
