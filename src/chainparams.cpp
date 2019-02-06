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
    std::make_pair("83e3a91e833226b61525044843f09462ac60b318", 10000 * UNIT),
    std::make_pair("505119f3fe6cbf76ab1f7328857eb52f0937e590", 10000 * UNIT),
    std::make_pair("e8906ab31d610ada80112899b715e6dbb8d27423", 10000 * UNIT),
    std::make_pair("5d40f7842ff3f30aec0c5fea0a9d033d229a5d29", 10000 * UNIT),
    std::make_pair("db12a44b98122b859e92e6b46fa66fdd667496fb", 10000 * UNIT),
    std::make_pair("ede0eea1c6c17b10a85944af563e508fca49219b", 10000 * UNIT),
    std::make_pair("61233f299916c82507e5277a97acbf5adf91b968", 10000 * UNIT),
    std::make_pair("65b78ea46c662126f8021e3b752eddd8e8e660d8", 10000 * UNIT),
    std::make_pair("ad2585d4ccdde1dc664ff3ea8739493905c4fd1b", 10000 * UNIT),
    std::make_pair("96363f7d9307154445f134fd179fc61d66dba92b", 10000 * UNIT),
    std::make_pair("4b7851a885dec0ac9976d16d536d2fc889ce2846", 10000 * UNIT),
    std::make_pair("5b820c412d5db51e5a4a1fe1e7207a78994e38c2", 10000 * UNIT),
    std::make_pair("926840bb7937c7711857890470794cabd714d3ac", 10000 * UNIT),
    std::make_pair("8358aff2ed6a0ea669fec8dde1ed61d462a284e6", 10000 * UNIT),
    std::make_pair("53976a0de21132231a9aa9b322c725dba67ec824", 10000 * UNIT),
    std::make_pair("7340f53353897b6bcd5c238fc0b02f27cd767e6c", 10000 * UNIT),
    std::make_pair("6f3f3352bba6e48a9e1fdca76e52e2378a6dbc40", 10000 * UNIT),
    std::make_pair("4ada36decfa1db30b42cf9ffc67272a988a42cdc", 10000 * UNIT),
    std::make_pair("c1755fd94603d1836278906b5d55e2922e8b79d8", 10000 * UNIT),
    std::make_pair("456db53de2131156c98bdb0918e49bdd51fae53a", 10000 * UNIT),
    std::make_pair("db63037723e9d21844aac2e1e360d3740eb0c9d4", 10000 * UNIT),
    std::make_pair("5e0a820ab239cdd2fc911ba8cba542df955d1dae", 10000 * UNIT),
    std::make_pair("cbe2b2a7c91228992275200e3685bd751cb8be76", 10000 * UNIT),
    std::make_pair("d4f62818acab3c2fa54451b14fa03158d1593768", 10000 * UNIT),
    std::make_pair("8c357cb666a569d70eeaeb7e6ecc7cf552a2f8b4", 10000 * UNIT),
    std::make_pair("634aa8109568e2cbde00bdf3e352f05f0da2b681", 10000 * UNIT),
    std::make_pair("ef8a14f44fd15afa347f1d2d2d7288b42256b3e7", 10000 * UNIT),
    std::make_pair("b86351b8f67d2d6ffbd9bbb1008961287fcf2503", 10000 * UNIT),
    std::make_pair("d3b1a6b3b98af30f84287cdab5cc7c2856846cd4", 10000 * UNIT),
    std::make_pair("a9ebbd7c0713caaf313b98ce4b5656c7ca14fb6a", 10000 * UNIT),
    std::make_pair("d3c271832eb5083af408c249c49cb07e4612b069", 10000 * UNIT),
    std::make_pair("9fe82f6ef00c4d6cdbfaa41c6180b6e36ad9d792", 10000 * UNIT),
    std::make_pair("bba99a0e96453afed3de6a2bdab2399db3da0fae", 10000 * UNIT),
    std::make_pair("1c0799afeeb1e3be8728ddd2c10e7392d834570b", 10000 * UNIT),
    std::make_pair("00b33e099a7c79f2129fed0125d59df0cd25ff89", 10000 * UNIT),
    std::make_pair("5aaf228370e1dfdb0324374f4b2880a45ecd4fb8", 10000 * UNIT),
    std::make_pair("5f4e224d91e2a58156d301e89a5a013a58b910a2", 10000 * UNIT),
    std::make_pair("625ecd0f70b5bff53e7127bbf07453e51b6839ec", 10000 * UNIT),
    std::make_pair("995dc4bf6b962fbf4d387a2324d18831d2a4d8f8", 10000 * UNIT),
    std::make_pair("7092ccc063d027f8764cf9f3beba3e4e281a1557", 10000 * UNIT),
    std::make_pair("b452e96a6ed155fc35fea82cdfe4a33bfdf28a88", 10000 * UNIT),
    std::make_pair("e2528e31be5b3f10ed3ac03ca447f118cd20d37d", 10000 * UNIT),
    std::make_pair("c6d1219152972b5b6fcc92050578ff7f72f16e9a", 10000 * UNIT),
    std::make_pair("13f7df5944f9d1579d433c2a91ee90c9a46756ee", 10000 * UNIT),
    std::make_pair("ec06dedfde74f4739d6d11c9d570e1309f801db8", 10000 * UNIT),
    std::make_pair("0aacd3769006e2f70572f0309bb6cb2d2b0d0160", 10000 * UNIT),
    std::make_pair("16b3a3bce0871871069c6c909c4331ff564a15ef", 10000 * UNIT),
    std::make_pair("fc3f2faea7780ba88f5848fa8ed45106687f66f0", 10000 * UNIT),
    std::make_pair("f62beca488f8bc5a7963c85ac3d18ad29cf6ca65", 10000 * UNIT),
    std::make_pair("b756ad9f2738fa6543b3c5532adf09b7d979e053", 10000 * UNIT),
    std::make_pair("9398bf67345b5c0a48a22685ea4f107f7482018d", 10000 * UNIT),
    std::make_pair("bb98bb24c53334151246e6a6beed7978dd4adff8", 10000 * UNIT),
    std::make_pair("b78b4def5453b032ebaedeb8423c3f95a422bc51", 10000 * UNIT),
    std::make_pair("50109ec7a561bdfdb9a538ef9bf23d5e14b27559", 10000 * UNIT),
    std::make_pair("d1bc3f049bd9bd35fe037bfb824e69775159838d", 10000 * UNIT),
    std::make_pair("26b1e836b9095c60cfdd3e800563e7f2b20e320c", 10000 * UNIT),
    std::make_pair("004ebc3810db528ffebddb77b1a8a554ffc147c6", 10000 * UNIT),
    std::make_pair("be01d1d3d5adb8bdeeb19aeec216f8269f1a7e93", 10000 * UNIT),
    std::make_pair("e9624b46c6633e60db76b12dd20487b38ee9d541", 10000 * UNIT),
    std::make_pair("08707ccd1b5644ca4c887432d0ae4acee81cd8fa", 10000 * UNIT),
    std::make_pair("b7172c2ce209f32701f21f66faa221325e24855b", 10000 * UNIT),
    std::make_pair("35b2840f25f44c905055c9382378e154e9ce2b70", 10000 * UNIT),
    std::make_pair("ce741ae593bfc161109529944929d2c911d024a7", 10000 * UNIT),
    std::make_pair("ab06eddf80603bb6cc6f3e8df3915a334f6c97f3", 10000 * UNIT),
    std::make_pair("f0e01c5f290ceb3f02da722b030c0146c1ad6316", 10000 * UNIT),
    std::make_pair("5b9bbfbcb543168bcecbb08720e48cc3e7e40ac8", 10000 * UNIT),
    std::make_pair("82cb0fb25d62b72d09cb6fcbe5788933982f62a2", 10000 * UNIT),
    std::make_pair("0e656ebb4d3951f2870fe9feb794ced50ad1ca6d", 10000 * UNIT),
    std::make_pair("a6e7b521bc141821a562cba1684417ecc1ec1381", 10000 * UNIT),
    std::make_pair("725eb30f350ac0969fb8ff9bcdc375133ba00cb2", 10000 * UNIT),
    std::make_pair("4b3b96a7e9dee6797efeb4c8946b0d1852a235b5", 10000 * UNIT),
    std::make_pair("6343080b558a179f677d48bf9af7797c063016d5", 10000 * UNIT),
    std::make_pair("db329f2c824764863a26b1ca087b0854600661c9", 10000 * UNIT),
    std::make_pair("b28fd4c0158b772df8f648493b91be17b4eb26c1", 10000 * UNIT),
    std::make_pair("4d33bf1d87facf3e98549c48473c961672ffa3a9", 10000 * UNIT),
    std::make_pair("c0ad869f1ec51d3535eb04b3a4d45a5fefad0906", 10000 * UNIT),
    std::make_pair("a37cd2bd172dc2299dfee1e5ad75a219691d4994", 10000 * UNIT),
    std::make_pair("a351f95724f7c68adcda34eb063952add4372c64", 10000 * UNIT),
    std::make_pair("fc05908baefc1f8d68c07725b6ec2955523cfe94", 10000 * UNIT),
    std::make_pair("beafa504eddd9d3b5a541bcfca27a39bfd397e1e", 10000 * UNIT),
    std::make_pair("8cd4456c37b319881085aec63e02308d67681eb0", 10000 * UNIT),
    std::make_pair("2b4d2b864136920c01009ef2abec93fd6060c269", 10000 * UNIT),
    std::make_pair("59a1b3a4cd6d8bc046ec5ed5b74ed4d47c0ef8d2", 10000 * UNIT),
    std::make_pair("9a8ca0fc94b4be602a0ff3e9b376318395608a56", 10000 * UNIT),
    std::make_pair("50a204c1cd9e06d460685ecd67632a8e49cd04f6", 10000 * UNIT),
    std::make_pair("43ab5d854b5745a8eed8420b64e59a5fde11767d", 10000 * UNIT),
    std::make_pair("40ad9893bc764c3d3c5b5ac84a8e190997aac59d", 10000 * UNIT),
    std::make_pair("be461372ec08c7e9df2d1d274e42e4ca5f3ed038", 10000 * UNIT),
    std::make_pair("a947bae3abef4edf4fa7ef2fb1e569ee09216082", 10000 * UNIT),
    std::make_pair("f8c3bfd3811dea987c145177c28828d847fd0c79", 10000 * UNIT),
    std::make_pair("13704853c1e2d3f8289d8bad7acc6307eb8813de", 10000 * UNIT),
    std::make_pair("b906667bcd14e2a5a652bfa1095900c019f28d50", 10000 * UNIT),
    std::make_pair("0e8d559eecbf2fc612823ebcc0caa52a707eb8cd", 10000 * UNIT),
    std::make_pair("295a801bd8605c361ba46748bf9be5c3981c171b", 10000 * UNIT),
    std::make_pair("36cefad2c9c0104d9e9b0bf8881bc57c8240a09c", 10000 * UNIT),
    std::make_pair("e6086d273c53694f66cee29528954d5fbfb83c37", 10000 * UNIT),
    std::make_pair("fb22a8082aed94791282b2f18b3720e0a6de702f", 10000 * UNIT),
    std::make_pair("d6ff05b5a2fc23ba81b788c361f431d4ffd13b96", 10000 * UNIT),
    std::make_pair("3f3af228af19534debac7a9a06bdbbd30f48f76b", 10000 * UNIT),
    std::make_pair("cc08442b975d24615443e555b61d6fdf575b3fad", 10000 * UNIT),
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
        assert(consensus.hashGenesisBlock == uint256S("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

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
    CTestNetParams() {
        strNetworkID = "test";
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

        pchMessageStart[0] = 0xfd;
        pchMessageStart[1] = 0xfc;
        pchMessageStart[2] = 0xfb;
        pchMessageStart[3] = 0xfa;
        nDefaultPort = 17182;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1296688602, 414098458, 0x1d00ffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("test-seed.thirdhash.com");

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

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
    CRegTestParams() {
        strNetworkID = "regtest";
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

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 17292;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlockRegTest(1296688602, 7, 0x207fffff, 1, 50 * UNIT);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x1bdb1020e3ddb8f08987a7520b48a882266d2b7c69baf7fc17be2aa98cfd0065"));
        assert(genesis.hashMerkleRoot == uint256S("0x2dad876fa2439f764e0622cf95317ece238f0e03867c9d78d7744df789f6654e"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        adminParams.m_blockToAdminKeys.emplace(0, CreateRegTestAdminKeys());

        snapshotParams.create_snapshot_per_epoch = static_cast<uint16_t>(gArgs.GetArg("-createsnapshot", 1));
        snapshotParams.snapshot_chunk_timeout_sec = static_cast<uint16_t>(gArgs.GetArg("-snapshotchunktimeout", 5));
        snapshotParams.discovery_timeout_sec = static_cast<uint16_t>(gArgs.GetArg("-snapshotdiscoverytimeout", 5));

        // Initialize with default values for regTest
        finalization = esperanza::FinalizationParams();
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
