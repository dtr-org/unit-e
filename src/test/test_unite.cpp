// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>

#include <blockchain/blockchain_behavior.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <crypto/sha256.h>
#include <esperanza/finalizationstate.h>
#include <finalization/vote_recorder.h>
#include <injector.h>
#include <validation.h>
#include <miner.h>
#include <net_processing.h>
#include <ui_interface.h>
#include <streams.h>
#include <rpc/server.h>
#include <rpc/register.h>
#include <script/sigcache.h>
#include <snapshot/messages.h>

#include <memory>

void CConnmanTest::AddNode(CNode& node, CConnman* connman)
{
    LOCK(cs_vNodes);
    vNodes.push_back(&node);
}

void CConnmanTest::ClearNodes(CConnman* connman)
{
    LOCK(cs_vNodes);
    for (CNode* node : vNodes) {
        delete node;
    }
    vNodes.clear();
}

void CConnmanTest::StartThreadMessageHandler(CConnman* connman) {
    ThreadMessageHandler();
}

void SelectNetwork(const std::string& network_name) {
  auto name = network_name.c_str();
  auto network = blockchain::Network::_from_string(name);
  blockchain::Behavior::SetGlobal(blockchain::Behavior::NewForNetwork(network));
}

uint256 insecure_rand_seed = GetRandHash();
FastRandomContext insecure_rand_ctx(insecure_rand_seed);

extern bool fPrintToConsole;
extern void noui_connect();

std::ostream& operator<<(std::ostream& os, const uint256& num)
{
    os << num.ToString();
    return os;
}

ReducedTestingSetup::ReducedTestingSetup(const std::string& chainName)
{
    SHA256AutoDetect();
    RandomInit();
    ECC_Start();
    assert(snapshot::InitSecp256k1Context());
    snapshot::SnapshotIndex::Clear();
    SetupEnvironment();
    SetupNetworking();
    InitSignatureCache();
    InitScriptExecutionCache();
    fCheckBlockIndex = true;
    noui_connect();
}

ReducedTestingSetup::~ReducedTestingSetup()
{
    ECC_Stop();
    snapshot::DestroySecp256k1Context();
}

BasicTestingSetup::BasicTestingSetup(
    const std::string& chainName,
    UnitEInjectorConfiguration config)
    : ReducedTestingSetup(chainName),
      m_path_root(fs::temp_directory_path() / "test_unite" / strprintf("%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(1 << 30))))
{
    blockchain::Behavior::SetGlobal(blockchain::Behavior::NewForNetwork(blockchain::Network::_from_string(chainName.c_str())));
    config.use_in_memory_databases = true;
    UnitEInjector::Init(config);
    SelectParams(GetComponent<blockchain::Behavior>(), chainName);
}

fs::path BasicTestingSetup::SetDataDir(const std::string& name) {
    fs::path ret = m_path_root / name;
    fs::create_directories(ret);
    gArgs.ForceSetArg("-datadir", ret.string());
    return ret;
}

BasicTestingSetup::~BasicTestingSetup()
{
    fs::remove_all(m_path_root);
    UnitEInjector::Destroy();
}

TestingSetup::TestingSetup(const std::string& chainName, UnitEInjectorConfiguration config)
    : BasicTestingSetup(chainName, config)
{
    SetDataDir("tempdir");
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.

        RegisterAllCoreRPCCommands(tableRPC);
        ClearDatadirCache();

        // We have to run a scheduler thread to prevent ActivateBestChain
        // from blocking due to queue overrun.
        threadGroup.create_thread(boost::bind(&CScheduler::serviceQueue, &scheduler));
        GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

        finalization::VoteRecorder::DBParams params;
        params.inmemory = true;
        finalization::VoteRecorder::Reset(params);

        mempool.setSanityCheck(1.0);
        pblocktree.reset(new CBlockTreeDB(1 << 20, true));
        pcoinsdbview.reset(new CCoinsViewDB(1 << 23, true));
        pcoinsTip.reset(new CCoinsViewCache(pcoinsdbview.get()));
        if (!LoadGenesisBlock(chainparams)) {
            throw std::runtime_error("LoadGenesisBlock failed.");
        }
        {
            CValidationState state;
            if (!ActivateBestChain(state, chainparams)) {
                throw std::runtime_error(strprintf("ActivateBestChain failed. (%s)", FormatStateMessage(state)));
            }
        }
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        g_connman = std::unique_ptr<CConnman>(new CConnman(0x1337, 0x1337)); // Deterministic randomness for tests.
        connman = g_connman.get();
        peerLogic.reset(new PeerLogicValidation(connman, scheduler, /*enable_bip61=*/true));
}

TestingSetup::~TestingSetup()
{
        threadGroup.interrupt_all();
        threadGroup.join_all();
        GetMainSignals().FlushBackgroundCallbacks();
        GetMainSignals().UnregisterBackgroundSignalScheduler();
        g_connman.reset();
        peerLogic.reset();
        UnloadBlockIndex();
        pcoinsTip.reset();
        pcoinsdbview.reset();
        pblocktree.reset();
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction &tx) {
    CTransaction txn(tx);
    return FromTx(MakeTransactionRef(txn));
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransactionRef &txn) {
    return CTxMemPoolEntry(txn, nFee, nTime, nHeight,
                           spendsCoinbase, sigOpCost, lp);
}
