// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <blockchain/blockchain_parameters.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utilmoneystr.h>
#include <ufp64.h>

#include <assert.h>
#include <memory>

#include <chainparamsseeds.h>

const std::pair<const char*, CAmount> regTestOutputs[] = {
    std::make_pair("33a471b2c4d3f45b9ab4707455f7d2e917af5a6e", 10000 * UNIT),
    std::make_pair("7eac29a2e24c161e2d18d8d1249a6327d18d390f", 10000 * UNIT),
    std::make_pair("caca901140bf287eff2af36edeb48503cec4eb9f", 10000 * UNIT),
    std::make_pair("1f34ea7e96d82102b22afed6d53d02715f8f6621", 10000 * UNIT),
    std::make_pair("eb07ad5db790ee4324b5cdd635709f47e41fd867", 10000 * UNIT),

    // Check test/functional/test_framework/regtest_mnemonics.py to see the associated mnemonics
    std::make_pair("123e11b1002245baa28d5ad23b30dfe54f0dfa6d", 10000 * UNIT),
    std::make_pair("2c78252da8fbb60f70e737b57285ff807b463054", 10000 * UNIT),
    std::make_pair("a4bf9c5e1535c65889429dca2c8f8a96e0ad0680", 10000 * UNIT),
    std::make_pair("c358153f6176692b723f3f7841eaa4532a955adf", 10000 * UNIT),
    std::make_pair("c0ea55a2329f30f562dc765326155d6211450cb5", 10000 * UNIT),
    std::make_pair("14eb1d66667cf027e1e031b810c5d85ad0ea422e", 10000 * UNIT),
    std::make_pair("c011048dae8e78adef63bcd44af655fc0b0bae3c", 10000 * UNIT),
    std::make_pair("eba13431b66387b94a1943969ffae8778041e71c", 10000 * UNIT),
    std::make_pair("52a20f8eae92d8bea6164d1454a525130a69322c", 10000 * UNIT),
    std::make_pair("0d9de4504bb66ba37344e956f45074f2eb5ff6ed", 10000 * UNIT),
    std::make_pair("7d9da96b4f39d88fe42b5652a1149b204fc64aa9", 10000 * UNIT),
    std::make_pair("cc9f0507e8d38f2409e2d9d058824461ec73ee47", 10000 * UNIT),
    std::make_pair("07fe82d639e3f0301819bcff3d6a767279ae80c0", 10000 * UNIT),
    std::make_pair("2efee86a4236599d448142a49838be2d124dd3cc", 10000 * UNIT),
    std::make_pair("b83e6d3941324ea3ec5da0adb1f1e3892de2299e", 10000 * UNIT),
    std::make_pair("7d023ca654c12de44b53cfb69c2864e0e9d26993", 10000 * UNIT),
    std::make_pair("dd002ac11f8ec612ec103c45f3a13791ffb674a9", 10000 * UNIT),
    std::make_pair("09ff1c401891565b989448c1fe27e35bc7a8c351", 10000 * UNIT),
    std::make_pair("598a5739af96eda676ff00c480a2b7d0683f7537", 10000 * UNIT),
    std::make_pair("0fa51ecf43f4cdc9b21d6a0a114a01a9ff4b3aa7", 10000 * UNIT),
    std::make_pair("45f7a1972d4cf542ba3c27446a1a21ee7a4dd943", 10000 * UNIT),
    std::make_pair("a5d80fc916213ea5742cfc4b60bff8bff0d7a18e", 10000 * UNIT),
    std::make_pair("2dbd59d14a03d9b414f6f7a58cbfd002ec2deb6a", 10000 * UNIT),
    std::make_pair("b522dc3ebee526b9114a28c1dd4a49403f6f136e", 10000 * UNIT),
    std::make_pair("429860390ec073b24c5242fd163109e20a4fc81e", 10000 * UNIT),
    std::make_pair("1a0321c9120580a34fe5d4d9e49c91eff100adf7", 10000 * UNIT),
    std::make_pair("324e230d85307563f45349ff9000daef84344cb9", 10000 * UNIT),
    std::make_pair("f28ba03cdee7ceb1fabdb1498b814a64a54c1ee2", 10000 * UNIT),
    std::make_pair("067c440f405e2f8981f5842b20d1a72911416b2e", 10000 * UNIT),
    std::make_pair("a018a7e07a6d0dc992f3280917ba84eeb52038f5", 10000 * UNIT),
    std::make_pair("9e8f21197b73ae058c6bff6c7c83cea03cb6d768", 10000 * UNIT),
    std::make_pair("54b355306866752293760b200a24bed62a340d0f", 10000 * UNIT),
    std::make_pair("24dcb8499304497208be87b82edd6899b5e1c0c2", 10000 * UNIT),
    std::make_pair("68f4c71bb9bc40402ac75b306e404135707800fc", 10000 * UNIT),
    std::make_pair("65b7e2449fc2862c977fd824d6acc79c6723228e", 10000 * UNIT),
    std::make_pair("34901a28a878e3cbdb9c2bf1749b739c78585997", 10000 * UNIT),
    std::make_pair("4f577d4cb38e0be1efc91df5413031fb3a021345", 10000 * UNIT),
    std::make_pair("5d176aba621182f3930708ceadb18a9c6fc318e8", 10000 * UNIT),
    std::make_pair("a91f97c6ec359fe359865ceeadc869945d5b097f", 10000 * UNIT),
    std::make_pair("f63e1f48e66b846450ed62c12819a43716d2330f", 10000 * UNIT),
    std::make_pair("f383709610286788640017efeae26271ca737569", 10000 * UNIT),
    std::make_pair("c5412f2f07ebcf49e4799d3d38f241ad8aaf51c6", 10000 * UNIT),
    std::make_pair("1bbf69b8a2b27c7ddaa482c6a3d8e8d126156219", 10000 * UNIT),
    std::make_pair("dfb28d749e1be27ef112fbcc1c87a28bfa996bb2", 10000 * UNIT),
    std::make_pair("5f09b4af359d2c24d8720357010cc69d1b556af6", 10000 * UNIT),
    std::make_pair("3911035bf46109d3805bba41838c7137c06711ac", 10000 * UNIT),
    std::make_pair("c8cabc37c15d8d03fdef98e2c37471b95845da95", 10000 * UNIT),
    std::make_pair("2361a975a21adfa55b076671912ed6daf28f2ed3", 10000 * UNIT),
    std::make_pair("e2c6cca9e56a7196678c625ab2cc466178a51397", 10000 * UNIT),
    std::make_pair("f7bc52f218171c04adedb321436ca6bbce9a0976", 10000 * UNIT),
    std::make_pair("3f1e33a633d0fe0bc988629d106d26d102e9eae5", 10000 * UNIT),
    std::make_pair("2be8b3b68d9827ce9cb43cbcfbe4644b7221312a", 10000 * UNIT),
    std::make_pair("9bfd819d76177cdf8aec62de31609c924f71d69a", 10000 * UNIT),
    std::make_pair("09b4eeaf6c3c264af48804295bf1ebcd56311e06", 10000 * UNIT),
    std::make_pair("777a559c01d71b9607e804cd32ba046457fc3a2b", 10000 * UNIT),
    std::make_pair("ef8fd5d1446f9e4829e91838b47b5aa10ca0fd0b", 10000 * UNIT),
    std::make_pair("b4f669a167d895f441b7fd6772aa473bc6ee7f48", 10000 * UNIT),
    std::make_pair("120dd1780927247566c1a410716271f96c4ef5b4", 10000 * UNIT),
    std::make_pair("f77b2a995f994902c46bdd7b7f7e7ac519d0de29", 10000 * UNIT),
    std::make_pair("52ad376e9a8fce7b5a91ca1c3027586a4c853c01", 10000 * UNIT),
    std::make_pair("98a35b29eb5e83255241e4adfb967ec8da106aaa", 10000 * UNIT),
    std::make_pair("2b1a61b7e47cd3cad5a38926cda723c22b321e1a", 10000 * UNIT),
    std::make_pair("1c655020505e85072a880074b6ec1fa8f5428330", 10000 * UNIT),
    std::make_pair("3986a917ac63c1872759972ce826e0c48aece7a8", 10000 * UNIT),
    std::make_pair("624dcc492715889b8a3bbead1b10e49ee8e93337", 10000 * UNIT),
    std::make_pair("58da2c39dbac9638f002cf552599cb1f9e3eecd8", 10000 * UNIT),
    std::make_pair("b0ab64e1fbe197ae9a372f05d50ef93e64d2a957", 10000 * UNIT),
    std::make_pair("4af788134a587dc1721d330235a0a91ce3c55404", 10000 * UNIT),
    std::make_pair("11ad8a1387b96d068a8f7519a98933c2c9ea0940", 10000 * UNIT),
    std::make_pair("b9385684b813607679dce681c648bae76c0ea5fe", 10000 * UNIT),
    std::make_pair("61d21b1d9ebe0874a62262ac9bb96b0c29e3d614", 10000 * UNIT),
    std::make_pair("cf4af742bc7e4b09be80708f2f6d41f513e7ee84", 10000 * UNIT),
    std::make_pair("cae3c2ffa49e9252232e2112ef123887eacba751", 10000 * UNIT),
    std::make_pair("37d7c5990ab5b3bf44cb889a47d4c61d996f3d2a", 10000 * UNIT),
    std::make_pair("be067af3c4a8237c33486ae9ce1a4f8d3fe5a102", 10000 * UNIT),
    std::make_pair("a5a2140402346069b2edf1813184e94196f4eaa8", 10000 * UNIT),
    std::make_pair("c8d1bc9b668fc4afe37c718f8e6fc1ef104e2e5b", 10000 * UNIT),
    std::make_pair("b7179c9946c9bf49f1e1a96aaeba4b7cfa601735", 10000 * UNIT),
    std::make_pair("8e47f829b98adbd9cd06b2d0c79420f8729f9243", 10000 * UNIT),
    std::make_pair("8f4104d141fe01c9d0a0ae55627650009151ed95", 10000 * UNIT),
    std::make_pair("2cde5ae0eb43f001ac6663813d74cfa4ef380892", 10000 * UNIT),
    std::make_pair("71d81e0433cf491b7f9d0572872d423fe216fb53", 10000 * UNIT),
    std::make_pair("1faa4ffa1eb2100f4c56086a030c38f4bc654ca8", 10000 * UNIT),
    std::make_pair("0f88c67abafcf665fadbb9850cec774ef1a4ec9b", 10000 * UNIT),
    std::make_pair("0919e3c62f0ac747aab7a4a591773171d7231fd7", 10000 * UNIT),
    std::make_pair("35d9c169873dc833355d6f0795d66933eb23e2c8", 10000 * UNIT),
    std::make_pair("96996943e3e9814b9e6c3b0664d643e4d1a40abf", 10000 * UNIT),
    std::make_pair("ff8018fa112e7adc7c52cf6c05459fc81b3ebd8a", 10000 * UNIT),
    std::make_pair("a36cebe652f8689ec6bdf98f8fba3e3f253a5591", 10000 * UNIT),
    std::make_pair("97c63eb92a797a7312fc4f43b6d5c4d727871548", 10000 * UNIT),
    std::make_pair("22bad86540fef03edb1a753faf0bbe8408887465", 10000 * UNIT),
    std::make_pair("bc512016bf93b1d263a7e70d681cc9a7adc2f983", 10000 * UNIT),
    std::make_pair("97cc53271c6e0f8f0394eeb593bfd0f35d5a0b75", 10000 * UNIT),
    std::make_pair("eb0e594dc54691c828a94d99a4f2ccb36f400c43", 10000 * UNIT),
    std::make_pair("73fbde5fb997f180b42412e3b7af95f1bacb8184", 10000 * UNIT),
    std::make_pair("f3f8622584478ad23066314998ef471d6ad6a968", 10000 * UNIT),
    std::make_pair("28450d93c2b554d787c17b719a70cea902224205", 10000 * UNIT),
    std::make_pair("435acf0bb5451438113053defbfc04783493a325", 10000 * UNIT),
    std::make_pair("4b1142981e9d6fbf0133c8cac682abbb96757de8", 10000 * UNIT),
    std::make_pair("50ef997eb53326281f7c52191c3abbebf4b5de53", 10000 * UNIT),
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

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        parameters = blockchain::Parameters::MainNet;
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

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000000000000005214481d2d96f898e3d5416e43359c145944a909d242e0"); //506067

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xee;
        pchMessageStart[1] = 0xee;
        pchMessageStart[2] = 0xae;
        pchMessageStart[3] = 0xc1;
        nDefaultPort = 7182;
        nPruneAfterHeight = 100000;

//        genesis = CreateGenesisBlock(1231006505, 0x1d00ffff, 1, 50 * UNIT);
//        consensus.hashGenesisBlock = genesis.GetHash();
        // UNIT-E TODO
//        assert(consensus.hashGenesisBlock == uint256S("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
//        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they dont support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("mainnet-seed.thirdhash.com");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "bc";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                { 11111, uint256S("0x0000000069e244f73d78e8fd29ba2fd2ed618bd6fa2ee92559f542fdb26e7c1d")},
                { 33333, uint256S("0x000000002dd5588a74784eaa7ab0507a18ad16a236e7b1ce69f00d7ddfb5d0a6")},
                { 74000, uint256S("0x0000000000573993a3c9e41ce34471c079dcf5f52a0e824a81e7f953b8661a20")},
                {105000, uint256S("0x00000000000291ce28027faea320c8d2b054b2e0fe44a773f3eefb151d6bdc97")},
                {134444, uint256S("0x00000000000005b12ffd4cd315cd34ffd4a594f430ac814c91184a0d42d2b0fe")},
                {168000, uint256S("0x000000000000099e61ea72015e79632f216fe6cb33d7899acb35b75c8303b763")},
                {193000, uint256S("0x000000000000059f452a5f7340de6682a977387c17010ff6e6c3bd83ca8b1317")},
                {210000, uint256S("0x000000000000048b95347e83192f69cf0366076336c639f9b7228e9ba171342e")},
                {216116, uint256S("0x00000000000001b4f4b433e81ee46494af945cf96014816a4e2370f11b23df4e")},
                {225430, uint256S("0x00000000000001c108384350f74090433e7fcf79a606b8e797f065b130575932")},
                {250000, uint256S("0x000000000000003887df1f29024b06fc2200b55f8af8f35453d7be294df2d214")},
                {279000, uint256S("0x0000000000000001ae8c72a0b0c301f67e3afca10e819efa9041e458e9bd7e40")},
                {295000, uint256S("0x00000000000000004d9b4ef50f0f9d686fd69db2e03af35a100370c64632a983")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block 0000000000000000002d6cca6761c99b3c2e936f9a0e304b7c7651a993f461de (height 506081).
            1516903077, // * UNIX timestamp of last known number of transactions
            295363220,  // * total number of transactions between genesis and that timestamp
                        //   (the tx=... number in the SetBestChain debug.log lines)
            3.5         // * estimated number of transactions per second after that timestamp
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        parameters = blockchain::Parameters::TestNet;
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

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x0000000002e9e7b00e1f6dc5123a04aad68dd0f0968d8c7aa45f6640795c37b1"); //1135275

        pchMessageStart[0] = 0xfd;
        pchMessageStart[1] = 0xfc;
        pchMessageStart[2] = 0xfb;
        pchMessageStart[3] = 0xfa;
        nDefaultPort = 17182;
        nPruneAfterHeight = 1000;

// UNIT-E TODO
//        genesis = CreateGenesisBlock(1296688602, 0x1d00ffff, 1, 50 * UNIT);
//        consensus.hashGenesisBlock = genesis.GetHash();
//        assert(consensus.hashGenesisBlock == uint256S("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"));
//        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("test-seed.thirdhash.com");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {546, uint256S("000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70")},
            }
        };

        chainTxData = ChainTxData{
            // Data as of block 000000000000033cfa3c975eb83ecf2bb4aaedf68e6d279f6ed2b427c64caff9 (height 1260526)
            1516903490,
            17082348,
            0.09
        };

    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        parameters = blockchain::Parameters::RegTest;
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

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 17292;
        nPruneAfterHeight = 1000;

// UNIT-E TODO
//        genesis = CreateGenesisBlockRegTest(1296688602, 0x207fffff, 1, 50 * UNIT);
//        consensus.hashGenesisBlock = genesis.GetHash();
//        assert(consensus.hashGenesisBlock == uint256S("0x59447f6b31d072aaeb1e08dc0df8772d53f39e5d1afe0494294af566227398de"));
//        assert(genesis.hashMerkleRoot == uint256S("0x60793ec2bde997c3279fbd53e06d7a08d26fd9ce90f9e8fa82e8e35b141687da"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0x0fc0564d6a9b524590b0f79d34a2453fdb962b438b1f6abe119f0d79c1484036")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        finalization.m_epochLength = 5;
        finalization.m_minDepositSize = 10000 * UNIT;
        finalization.m_dynastyLogoutDelay = 2;
        finalization.m_withdrawalEpochDelay = 5;
        finalization.m_slashFractionMultiplier = 3;
        finalization.m_bountyFractionDenominator = 25;
        finalization.m_baseInterestFactor = ufp64::to_ufp64(700);
        finalization.m_basePenaltyFactor = ufp64::div_2uint(2, 100000);

        adminParams.m_blockToAdminKeys.emplace(0, CreateRegTestAdminKeys());

        snapshotParams.create_snapshot_per_epoch = static_cast<uint16_t>(gArgs.GetArg("-createsnapshot", 1));
        snapshotParams.fast_sync_timeout_sec = 5;

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";
    }
};

void CChainParams::UpdateFinalizationParams(esperanza::FinalizationParams &params) {

  if (strNetworkID == "regtest") {
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
