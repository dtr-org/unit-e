// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utilmoneystr.h>
#include <ufp64.h>

#include <assert.h>
#include <memory>

#include <chainparamsseeds.h>

esperanza::AdminKeySet CreateAdminKeys(std::array<std::string, esperanza::ADMIN_MULTISIG_KEYS>&& pubkeys) {
    esperanza::AdminKeySet key_set;
    for (int i = 0; i < pubkeys.size(); ++i) {
        std::vector<unsigned char> key_data = ParseHex(std::move(pubkeys[i]));
        CPubKey key(key_data);
        assert(key.IsValid());
        key_set[i] = std::move(key);
    }

    return key_set;
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams(const blockchain::Parameters &params) : CChainParams(params) {
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1554901200;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1560250245;

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256::zero;

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256::zero;

        genesis = parameters.genesis_block.block;
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("test-seed.thirdhash.com");

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;

        adminParams.admin_keys = CreateAdminKeys({
            "02630a75cd35adc6c44ca677e83feb8e4a7e539baaa49887c455e8242e3e3b1c05",
            "03946025d10e3cdb30a9cd73525bc9acc4bd92e184cdd9c9ea7d0ebc6b654afcc5",
            "0290f45494a197cbd389181b3d7596a90499a93368159e8a6e9f9d0d460799d33d"
        });

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        finalization.epoch_length = 50;
        finalization.min_deposit_size = 10000 * UNIT;
        finalization.dynasty_logout_delay = 700;
        finalization.withdrawal_epoch_delay = static_cast<int>(1.5e4);
        finalization.slash_fraction_multiplier = 3;
        finalization.bounty_fraction_denominator = 25;
        finalization.base_interest_factor = ufp64::to_ufp64(7);
        finalization.base_penalty_factor = ufp64::div_2uint(2, 10000000);
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams(const blockchain::Parameters& params) : CChainParams(params) {
        consensus.nSubsidyHalvingInterval = 150;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256::zero;

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256::zero;

        genesis = parameters.genesis_block.block;
        consensus.hashGenesisBlock = genesis.GetHash();

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        if(gArgs.GetBoolArg("-permissioning", false)) {
            adminParams.admin_keys = CreateAdminKeys({
                "038c0246da82d686e4638d8cf60452956518f8b63c020d23387df93d199fc089e8",
                "02f1563a8930739b653426380a8297e5f08682cb1e7c881209aa624f821e2684fa",
                "03d2bc85e0b035285add07680695cb561c9b9fbe9cb3a4be4f1f5be2fc1255944c"
            });
        }

        snapshotParams.create_snapshot_per_epoch = static_cast<uint16_t>(gArgs.GetArg("-createsnapshot", 1));
        snapshotParams.snapshot_chunk_timeout_sec = static_cast<uint16_t>(gArgs.GetArg("-snapshotchunktimeout", 5));
        snapshotParams.discovery_timeout_sec = static_cast<uint16_t>(gArgs.GetArg("-snapshotdiscoverytimeout", 5));

        // Initialize with default values for regTest
        finalization = esperanza::FinalizationParams();
    }
};

void CChainParams::UpdateFinalizationParams(esperanza::FinalizationParams &params) {

  if (NetworkIDString() == "regtest") {
    finalization = params;
  }
}

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(Dependency<blockchain::Behavior> blockchain_behavior, const std::string& chain)
{
    if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams(blockchain_behavior->GetParameters()));
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(blockchain_behavior->GetParameters()));
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
  std::unique_ptr<blockchain::Behavior> blockchain_behavior;
  if (chain == CBaseChainParams::TESTNET) {
    blockchain_behavior = blockchain::Behavior::NewForNetwork(blockchain::Network::test);
  } else if (chain == CBaseChainParams::REGTEST) {
    blockchain_behavior = blockchain::Behavior::NewForNetwork(blockchain::Network::regtest);
  }
  if (!blockchain_behavior) {
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
  }
  return CreateChainParams(blockchain_behavior.get(), chain);
}

void SelectParams(Dependency<blockchain::Behavior> blockchain_behavior, const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(blockchain_behavior, network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}

void UpdateFinalizationParams(esperanza::FinalizationParams &params)
{
    globalChainParams->UpdateFinalizationParams(params);
}
