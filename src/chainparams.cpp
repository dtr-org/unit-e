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

static CBlock CreateGenesisBlockRegTest(uint32_t nTime,
                                        uint32_t nNonce,
                                        uint32_t nBits,
                                        int32_t nVersion,
                                        const CAmount &genesisReward) {

    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    CMutableTransaction txNew;
    txNew.nVersion = 1;
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
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 173805; // 00000000000000ce80a7e057163a4db1d5ad7b20fb6f598c9597b9665c8fb0d4 - April 1, 2012
        consensus.BIP34Height = 227931;
        consensus.BIP34Hash = uint256S("0xb808089c756add1591b1d17bab44bba3fed9e02f942ab4894b02000000000000");
        consensus.BIP65Height = 388381; // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        consensus.BIP66Height = 363725; // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.powLimit = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffff00000000");
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
        consensus.nMinimumChainWork = uint256S("0xcc7852bcd4ca579d571cf9000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xe042d209a94459149c35436e41d5e398f8962d1d481452000000000000000000"); //506067

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

        genesis = CreateGenesisBlock(1231006505, 2083236893, 0x1d00ffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000"));
        assert(genesis.hashMerkleRoot == uint256S("0x3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as a oneshot if they dont support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("mainnet-seed.thirdhash.com");

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                { 11111, uint256S("0x1d7c6eb2fd42f55925e92efad68b61edd22fba29fde8783df744e26900000000")},
                { 33333, uint256S("0xa6d0b5df7d0df069ceb1e736a216ad187a50b07aaa4e78748a58d52d00000000")},
                { 74000, uint256S("0x201a66b853f9e7814a820e2af5f5dc79c07144e31ce4c9a39339570000000000")},
                {105000, uint256S("0x97dc6b1d15fbeef373a744fee0b254b0d2c820a3ae7f0228ce91020000000000")},
                {134444, uint256S("0xfeb0d2420d4a18914c81ac30f494a5d4ff34cd15d34cfd2fb105000000000000")},
                {168000, uint256S("0x63b703835cb735cb9a89d733cbe66f212f63795e0172ea619e09000000000000")},
                {193000, uint256S("0x17138bca83bdc3e6f60f01177c3877a98266de40735f2a459f05000000000000")},
                {210000, uint256S("0x2e3471a19b8e22b7f939c63663076603cf692f19837e34958b04000000000000")},
                {216116, uint256S("0x4edf231bf170234e6a811460f95c94af9464e41ee833b4f4b401000000000000")},
                {225430, uint256S("0x32595730b165f097e7b806a679cf7f3e439040f750433808c101000000000000")},
                {250000, uint256S("0x14d2f24d29bed75354f3f88a5fb50022fc064b02291fdf873800000000000000")},
                {279000, uint256S("0x407ebde958e44190fa9e810ea1fc3a7ef601c3b0a0728cae0100000000000000")},
                {295000, uint256S("0x83a93246c67003105af33ae0b29dd66f689d0f0ff54e9b4d0000000000000000")},
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
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 514; // 00000000040b4e986385315e14bee30ad876d8b47f748025b26683116d21aa65
        consensus.BIP34Height = 21111;
        consensus.BIP34Hash = uint256S("0xf88ecd9912d00d3f5c2a8e0f50417d3e415c75b3abe584346da9b32300000000");
        consensus.BIP65Height = 581885; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 330776; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.powLimit = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffff00000000");
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
        consensus.nMinimumChainWork = uint256S("0x637dbb6df7b7da30280000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xb1375c7940665fa47a8c8d96f0d08dd6aa043a12c56d1f0eb0e7e90200000000"); //1135275

        pchMessageStart[0] = 0xfd;
        pchMessageStart[1] = 0xfc;
        pchMessageStart[2] = 0xfb;
        pchMessageStart[3] = 0xfa;
        nDefaultPort = 17182;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1296688602, 414098458, 0x1d00ffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x43497fd7f826957108f4a30fd9cec3aeba79972084e90ead01ea330900000000"));
        assert(genesis.hashMerkleRoot == uint256S("0x3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("test-seed.thirdhash.com");

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {546, uint256S("70cb6af7ebbcb1315d3414029c556c55f3e2fc353c4c9063a76c932a00000000")},
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
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Height = 0; // always enforce P2SH BIP16 on regtest
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f");
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

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 17292;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlockRegTest(1296688602, 5, 0x207fffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x364048c1790d9f11be6a1f8b432b96db3f45a2349df7b09045529b6a4d56c00f"));
        assert(genesis.hashMerkleRoot == uint256S("0x9752a960f975b3d1232f8a6f0c087bf4f3b7a7f264cc094c214f1448e433d888"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0x364048c1790d9f11be6a1f8b432b96db3f45a2349df7b09045529b6a4d56c00f")},
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
        snapshotParams.snapshot_chunk_timeout_sec = static_cast<uint16_t>(gArgs.GetArg("-snapshotchunktimeout", 5));
        snapshotParams.discovery_timeout_sec = static_cast<uint16_t>(gArgs.GetArg("-snapshotdiscoverytimeout", 5));
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
