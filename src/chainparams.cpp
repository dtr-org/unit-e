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

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.SetType(TxType::COINBASE);
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

const std::pair<const char*, CAmount> regTestOutputs[] = {

    // Output for Regtest
    std::make_pair("f920777dfcc391b6deb9635a363856d1ccfe9c15", 10000 * UNIT),

    std::make_pair("33a471b2c4d3f45b9ab4707455f7d2e917af5a6e", 10000 * UNIT),
    std::make_pair("7eac29a2e24c161e2d18d8d1249a6327d18d390f", 10000 * UNIT),
    std::make_pair("caca901140bf287eff2af36edeb48503cec4eb9f", 10000 * UNIT),
    std::make_pair("1f34ea7e96d82102b22afed6d53d02715f8f6621", 10000 * UNIT),
    std::make_pair("eb07ad5db790ee4324b5cdd635709f47e41fd867", 10000 * UNIT),

    // Check test/functional/test_framework/regtest_mnemonics.py to see the associated mnemonics
    std::make_pair("9871035b2352607356b030a4fa989dfc23582b48", 10000 * UNIT),
    std::make_pair("916112535557149be624bd284ee2fc96894137f4", 10000 * UNIT),
    std::make_pair("83561b7922283332a2cfbef9b2ef087ba462bbc0", 10000 * UNIT),
    std::make_pair("c2c28cd4df085d164ea0e4a2f8f9c5a4fbe86487", 10000 * UNIT),
    std::make_pair("fb0d328b38449f5a71dbec3a4545ad41b99cd733", 10000 * UNIT),
    std::make_pair("44cc0275d12fee19063a02d25a99ecf593e57ad9", 10000 * UNIT),
    std::make_pair("b367cf8952634bd966525a21b94aaef0290216f1", 10000 * UNIT),
    std::make_pair("f5a5502b33247d9da7cbb69f4b5b65f35445d06c", 10000 * UNIT),
    std::make_pair("5e87150f5c6e63e4149a709528176e56688f7aee", 10000 * UNIT),
    std::make_pair("62d3f46ba2f9320a7ec94d4bb03d3098bd77dcd1", 10000 * UNIT),
    std::make_pair("2a5d35dd5dc91bc81e1b67779fa858e5d4acd165", 10000 * UNIT),
    std::make_pair("4cc1c8059ce6e8e0124f3cc9676fbc985e68a4a0", 10000 * UNIT),
    std::make_pair("a76f3ff7af26c21c66edc9d13133905753d52ccd", 10000 * UNIT),
    std::make_pair("8d96a8406cbd98d6a85bca1f70b433aeaecf830d", 10000 * UNIT),
    std::make_pair("4f41ffd587aac0d537a488d66bf761d320272382", 10000 * UNIT),
    std::make_pair("eb2689386c5b3798a133ecdd1a7beaa33e017a48", 10000 * UNIT),
    std::make_pair("eeb223d4058b1a0cf7a1fe103d1e9e484af0ee8c", 10000 * UNIT),
    std::make_pair("c22f3075e61f6d86cbd3aeab0fdbd71adeba2b96", 10000 * UNIT),
    std::make_pair("7467faafdcf17e52344f45606bd156f41f53f6be", 10000 * UNIT),
    std::make_pair("edb9a9845348c136ce674c02ace630d3872e52aa", 10000 * UNIT),
    std::make_pair("4c80622bcc4bde9b6d6b08cff8854d15ee24be52", 10000 * UNIT),
    std::make_pair("4a0e4a66a19f588ddc4bbc2019fa0664a02d2c52", 10000 * UNIT),
    std::make_pair("6490c0f74e453e69c7da60ab1336daeda24cbfbc", 10000 * UNIT),
    std::make_pair("cc17834128bd212406ce672799de3b2cb53ea079", 10000 * UNIT),
    std::make_pair("f4fed5d4fb7467fc6842fe639b666b4909bf4f77", 10000 * UNIT),
    std::make_pair("19624f38193db0cccc6ca8ccb9d21a355515d521", 10000 * UNIT),
    std::make_pair("f6028bbe9a264e38dff913d8d53c1d493c37fc9f", 10000 * UNIT),
    std::make_pair("38362ef29e1396858991a3bd1b67bb79f34e7ed2", 10000 * UNIT),
    std::make_pair("cef92f02996aff8c13d4820f7344fab002330bb4", 10000 * UNIT),
    std::make_pair("43a6f0567cf08d0c71756df64a3dedf6a2bb47ae", 10000 * UNIT),
    std::make_pair("cf3f54d39d003dfce484b94824574def44991b03", 10000 * UNIT),
    std::make_pair("bc53585134dd9002445de3a3974fbf723c5b7c29", 10000 * UNIT),
    std::make_pair("168b2ec287488139d0ae301d8d4b33b12c1cf1e9", 10000 * UNIT),
    std::make_pair("01855383f1ff14e0e7b92ebfc9e6d2c9fa550c7e", 10000 * UNIT),
    std::make_pair("9f1f3ea626a48a5241f7dcb4da602069e87393b5", 10000 * UNIT),
    std::make_pair("eec10447bf900ccd6870997ce568aa1e7eb2e34e", 10000 * UNIT),
    std::make_pair("1cb98723246669820e2cbd8723d7f0c41870bb9a", 10000 * UNIT),
    std::make_pair("e1a11108b9cae8e19cf876dc4d84748ee409a660", 10000 * UNIT),
    std::make_pair("d858f9867228375b90db8df8de9ee8a0eaa39f90", 10000 * UNIT),
    std::make_pair("65a55005c8109a0fba7c47a6414085c9012d330b", 10000 * UNIT),
    std::make_pair("b99b83c1cea07c27a743d0440b698a7d59f88e08", 10000 * UNIT),
    std::make_pair("8b9a35903d581914fe6a20649e59dd3ac4fd6e6b", 10000 * UNIT),
    std::make_pair("727d36c16ef2eaafca5f623f326a695e8b0d32b1", 10000 * UNIT),
    std::make_pair("6c4befb218ac9dda622e70a4adb7a1dc87d9968f", 10000 * UNIT),
    std::make_pair("16803bd3bee41042929432f10e4b2a1d1444d6a6", 10000 * UNIT),
    std::make_pair("47a8d42d185c627499acc871601a8aad95fef230", 10000 * UNIT),
    std::make_pair("323ac3d898fb13976785c8f1a660b9f6984e290c", 10000 * UNIT),
    std::make_pair("19c6dd1dc9a5ed25f0686bac5fd401c7a98f206f", 10000 * UNIT),
    std::make_pair("e2ee1b990ae164e76d291788066fa8425b581fae", 10000 * UNIT),
    std::make_pair("94640f1564c1bd641a7788061ed5ee07c570d452", 10000 * UNIT),
    std::make_pair("2329a445f725b28f20574d3c900423b173565169", 10000 * UNIT),
    std::make_pair("22611be9cced7adac5983916b7779234ad0bb4b5", 10000 * UNIT),
    std::make_pair("6fa33d14f425f8fef034a6c730857d86c132b609", 10000 * UNIT),
    std::make_pair("42a0f6697f10e95706819a3b89f602e5c7453091", 10000 * UNIT),
    std::make_pair("85010bd03ee6e3ea77274eaa164a9679f24fb1e3", 10000 * UNIT),
    std::make_pair("ee9f6d9126b2c585cee4040efe35b78ea3f9ac63", 10000 * UNIT),
    std::make_pair("739c11b5c5a52525b0770a803d3cfbbc9f583d1a", 10000 * UNIT),
    std::make_pair("e13aef971e9cf2f207c127a99e1d13e958014eac", 10000 * UNIT),
    std::make_pair("ad7c47d82039cfa58522f9698fc81b73ef24c040", 10000 * UNIT),
    std::make_pair("60af2469eae72254c3114669084ca7f338a72ad1", 10000 * UNIT),
    std::make_pair("da5747f10eda74eae0bc598e993702ce6cb017ff", 10000 * UNIT),
    std::make_pair("8896a833eb8ff9f63f5f79d944feddadb8f0a135", 10000 * UNIT),
    std::make_pair("43c832a18630cc72038bef75d95fd4d57b33d1c9", 10000 * UNIT),
    std::make_pair("4d334234807174352a85ea2ba1466e805a2a028b", 10000 * UNIT),
    std::make_pair("8e3e318a47c072e5d61190f0f6d3a41ab6a8a839", 10000 * UNIT),
    std::make_pair("aa511c9edd9e306ac62733a482746190ca57e397", 10000 * UNIT),
    std::make_pair("5d1f4e526e3c9929dedfbac841ebacf699f63af3", 10000 * UNIT),
    std::make_pair("76642e142dbf055e16ad7ddc7391e0eaf4238781", 10000 * UNIT),
    std::make_pair("e6536ab45749d64d338a3aa57858455fcbb3ad30", 10000 * UNIT),
    std::make_pair("869c00b5988ca34390a4d1f3e6cbfa1ccc0f4fa8", 10000 * UNIT),
    std::make_pair("b7ef23e6cc772747c63396a37d4865534e12f909", 10000 * UNIT),
    std::make_pair("68768c6f3c8834911a58f5ade0c5d7e57cf73d8c", 10000 * UNIT),
    std::make_pair("adb123b2f7f6c543f8c67740c21fef616f95a2f7", 10000 * UNIT),
    std::make_pair("86c87ec56216a76af06cd3ffceecaca7dbe54cf9", 10000 * UNIT),
    std::make_pair("10dd9ae08b71ad6c583756183889192f74288bfb", 10000 * UNIT),
    std::make_pair("097ffc3c6c5d47a71519e064f3c936cca563704c", 10000 * UNIT),
    std::make_pair("b4e618d4ea4e958ea9f0a96065d1e2b9fd4a7477", 10000 * UNIT),
    std::make_pair("f3bf7a8099a4d7892aed654ff72f152ed680cb57", 10000 * UNIT),
    std::make_pair("9c2a466d7e0a376b0f9efdfd556c9b98b5c3f639", 10000 * UNIT),
    std::make_pair("2091a1148a36ad99cbf766ee192a5ddea2322917", 10000 * UNIT),
    std::make_pair("f6f952f6b16f19e7c6d1548f28dff367d26a2e92", 10000 * UNIT),
    std::make_pair("c20b64dcdbfa1d7cb71046c4a9d0b3fa34e3c042", 10000 * UNIT),
    std::make_pair("5e3333ebdd82bb2e791d338bc0edcc48ec1a7d8b", 10000 * UNIT),
    std::make_pair("12ecaab92a4fd89f0e72df0db9fe36313e991165", 10000 * UNIT),
    std::make_pair("3a1639a35d784c677db414443a659f454c69e67b", 10000 * UNIT),
    std::make_pair("74e0bd1c7a05a4df0c5ccd6b75a9fad69001f18e", 10000 * UNIT),
    std::make_pair("c6329ff82b17cc0986f7a39c9a6d4b4a6d0d37c2", 10000 * UNIT),
    std::make_pair("73b7858ee022e002ba6d0c2b2b3244cd4b153119", 10000 * UNIT),
    std::make_pair("7f796be6888df2b9371cdac6a385b7a1f79d523b", 10000 * UNIT),
    std::make_pair("bca2a4c93db74ec227da4ae0c6decab948a409bb", 10000 * UNIT),
    std::make_pair("5e10f117beafe6b781f4c44f00474cdb4149d258", 10000 * UNIT),
    std::make_pair("393a673d9e4df3b8b6259336cbf27ebba14910c3", 10000 * UNIT),
    std::make_pair("39a65f0e3e442af02c6e387d03cee16c6d13590c", 10000 * UNIT),
    std::make_pair("20db1c6d44028efd60c97f1ca1ada7feb25e51ed", 10000 * UNIT),
    std::make_pair("7b12a4cc12fcd6002ea3a4c73ee7c6572070455c", 10000 * UNIT),
    std::make_pair("2209fbedd60f41a4fb94716856d61d508cf280a4", 10000 * UNIT),
    std::make_pair("5d26cda16533ab790e85fb70d1ee52a31fab9edc", 10000 * UNIT),
    std::make_pair("fecce82e4f1dbd6e5f381683d471508c13c5220c", 10000 * UNIT),
    std::make_pair("a935dce8c9a88652388dbdd1f3786772515cdc6f", 10000 * UNIT),
    std::make_pair("eb83dd51b5daa0d4af62256adf283abb3485a355", 10000 * UNIT),
};
const size_t nGenesisOutputsRegtest = sizeof(regTestOutputs) / sizeof(regTestOutputs[0]);

esperanza::AdminKeySet CreateRegTestAdminKeys() {
  const auto key0Data = ParseHex(
      "038c0246da82d686e4638d8cf60452956518f8b63c020d23387df93d199fc089e8");

  const auto key1Data = ParseHex(
      "02f1563a8930739b653426380a8297e5f08682cb1e7c881209aa624f821e2684fa");

  const auto key2Data = ParseHex(
      "03d2bc85e0b035285add07680695cb561c9b9fbe9cb3a4be4f1f5be2fc1255944c");

  CPubKey key0(key0Data.begin(), key0Data.end());
  CPubKey key1(key1Data.begin(), key1Data.end());
  CPubKey key2(key2Data.begin(), key2Data.end());

  assert(key0.IsValid());
  assert(key1.IsValid());
  assert(key2.IsValid());

  return {{key0, key1, key2}};
}

static CBlock CreateGenesisBlockRegTest(uint32_t nTime,
                                        uint32_t nNonce,
                                        uint32_t nBits,
                                        int32_t nVersion,
                                        const CAmount &genesisReward) {

    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.SetType(TxType::COINBASE);
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    txNew.vout.resize(nGenesisOutputsRegtest);
    for (size_t k = 0; k < nGenesisOutputsRegtest; ++k) {
        CScript scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex(regTestOutputs[k].first) << OP_EQUALVERIFY << OP_CHECKSIG;
        CTxOut out{regTestOutputs[k].second, scriptPubKey};
        txNew.vout[k] = out;
    }

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */

class CMainParams : public CChainParams {
public:
    CMainParams() : CChainParams(blockchain::Parameters::MainNet()) {
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000; // November 15th, 2016.
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1510704000; // November 15th, 2017.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000f91c579d57cad4bc5278cc");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000000000000005214481d2d96f898e3d5416e43359c145944a909d242e0"); //506067

        nDefaultPort = 7182;

        genesis = CreateGenesisBlock(1231006505, 2083236893, 0x1d00ffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0318aa4d107203133b18021e82e04eac23f455f7ae20afbbd1d696cffbeabf8d"));
        assert(genesis.hashMerkleRoot == uint256S("0xf1ce78a24acd5f7234a28f68ef4b448067d3ecbba8d651129a927529937f8f80"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they dont support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("mainnet-seed.thirdhash.com");

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;

        chainTxData = ChainTxData{
            // Data as of block 0000000000000000002d6cca6761c99b3c2e936f9a0e304b7c7651a993f461de (height 506081).
            1516903077, // * UNIX timestamp of last known number of transactions
            295363220,  // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.5         // * estimated number of transactions per second after that timestamp
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
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() : CChainParams(blockchain::Parameters::TestNet()) {
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1493596800; // May 1st 2017

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00000000000000000000000000000000000000000000002830dab7f76dbb7d63");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000002e9e7b00e1f6dc5123a04aad68dd0f0968d8c7aa45f6640795c37b1"); //1135275

        nDefaultPort = 17182;

        genesis = CreateGenesisBlock(1296688602, 414098458, 0x1d00ffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xd6d268d0d84dc5e832bccb96c2949f926fc490bf2d9dfd2006346c518c3b57cf"));
        assert(genesis.hashMerkleRoot == uint256S("0xf1ce78a24acd5f7234a28f68ef4b448067d3ecbba8d651129a927529937f8f80"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("test-seed.thirdhash.com");

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;

        chainTxData = ChainTxData{
            // Data as of block 000000000000033cfa3c975eb83ecf2bb4aaedf68e6d279f6ed2b427c64caff9 (height 1260526)
            1516903490,
            17082348,
            0.09
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
    CRegTestParams() : CChainParams(blockchain::Parameters::RegTest()) {
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
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        nDefaultPort = 17292;

        genesis = CreateGenesisBlockRegTest(1296688602, 7, 0x207fffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x7213a844fb67eb79fe84befc196244690b6ecdaa0ae1cd6367df523c943dc259"));
        assert(genesis.hashMerkleRoot == uint256S("0x4b0b286e67861cf321a461b4e609d6fd7b29e1404b02d17a0345f1e89c2e79cb"));

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
          adminParams.m_blockToAdminKeys.emplace(0, CreateRegTestAdminKeys());
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

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}

void UpdateFinalizationParams(esperanza::FinalizationParams &params)
{
    globalChainParams->UpdateFinalizationParams(params);
}
