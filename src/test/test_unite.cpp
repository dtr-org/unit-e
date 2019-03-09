// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>

#include <blockchain/blockchain_behavior.h>
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
    LOCK(connman->cs_vNodes);
    connman->vNodes.push_back(&node);
}

void CConnmanTest::ClearNodes(CConnman* connman)
{
    LOCK(connman->cs_vNodes);
    connman->vNodes.clear();
}

void CConnmanTest::StartThreadMessageHandler(CConnman* connman) {
  connman->ThreadMessageHandler();
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
  fPrintToDebugLog = false; // don't want to write to debug.log file
  fCheckBlockIndex = true;
  noui_connect();
}

ReducedTestingSetup::~ReducedTestingSetup()
{
  ECC_Stop();
  snapshot::DestroySecp256k1Context();
}

BasicTestingSetup::BasicTestingSetup(const std::string& chainName) : ReducedTestingSetup(chainName)
{
        blockchain::Behavior::SetGlobal(blockchain::Behavior::NewForNetwork(blockchain::Network::_from_string(chainName.c_str())));
        UnitEInjector::Init();
        SelectParams(GetComponent<blockchain::Behavior>(), chainName);
}

fs::path BasicTestingSetup::SetDataDir(const std::string& name)
{
  fs::path ret = fs::temp_directory_path() / name;
  fs::create_directories(ret);
  gArgs.ForceSetArg("-datadir", ret.string());
  return ret;
}

BasicTestingSetup::~BasicTestingSetup()
{
        UnitEInjector::Destroy();
}

TestingSetup::TestingSetup(const std::string& chainName) : BasicTestingSetup(chainName)
{
    const CChainParams& chainparams = Params();
        // Ideally we'd move all the RPC tests to the functional testing framework
        // instead of unit tests, but for now we need these here.

        RegisterAllCoreRPCCommands(tableRPC);
        ClearDatadirCache();
        pathTemp = fs::temp_directory_path() / strprintf("test_unite_%lu_%i", (unsigned long)GetTime(), (int)(InsecureRandRange(100000)));
        fs::create_directories(pathTemp);
        gArgs.ForceSetArg("-datadir", pathTemp.string());

        // We have to run a scheduler thread to prevent ActivateBestChain
        // from blocking due to queue overrun.
        threadGroup.create_thread(boost::bind(&CScheduler::serviceQueue, &scheduler));
        GetMainSignals().RegisterBackgroundSignalScheduler(scheduler);

        auto state_repository = GetComponent<finalization::StateRepository>();
        state_repository->Reset(chainparams.GetFinalization(),
                                chainparams.GetAdminParams());

        finalization::VoteRecorder::Reset();

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
                throw std::runtime_error("ActivateBestChain failed.");
            }
        }
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        g_connman = std::unique_ptr<CConnman>(new CConnman(0x1337, 0x1337)); // Deterministic randomness for tests.
        connman = g_connman.get();
        peerLogic.reset(new PeerLogicValidation(connman, scheduler));
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
        fs::remove_all(pathTemp);
}


CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CMutableTransaction &tx) {
    CTransaction txn(tx);
    return FromTx(txn);
}

CTxMemPoolEntry TestMemPoolEntryHelper::FromTx(const CTransaction &txn) {
    return CTxMemPoolEntry(MakeTransactionRef(txn), nFee, nTime, nHeight,
                           spendsCoinbase, sigOpCost, lp);
}
