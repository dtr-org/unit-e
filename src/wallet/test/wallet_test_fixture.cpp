// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

#include <rpc/server.h>
#include <wallet/db.h>
#include <wallet/rpcvalidator.h>

WalletTestingSetup::WalletTestingSetup(const std::string& chainName)
    : WalletTestingSetup([](Settings& s){}, chainName) {}

WalletTestingSetup::WalletTestingSetup(std::function<void(Settings&)> f, const std::string& chainName)
    : TestingSetup(chainName)
{
    bitdb.MakeMock();

    bool fFirstRun;
    g_address_type = OUTPUT_TYPE_DEFAULT;
    g_change_type = OUTPUT_TYPE_DEFAULT;
    std::unique_ptr<CWalletDBWrapper> dbw(new CWalletDBWrapper(&bitdb, "wallet_test.dat"));

    f(settings);
    esperanza::WalletExtensionDeps deps;
    deps.settings = &settings;

    pwalletMain = MakeUnique<CWallet>(deps, std::move(dbw));
    pwalletMain->LoadWallet(fFirstRun);
    vpwallets.insert(vpwallets.begin(), &*pwalletMain);
    RegisterValidationInterface(pwalletMain.get());

    RegisterWalletRPCCommands(tableRPC);
    RegisterValidatorRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup()
{
    UnregisterValidationInterface(pwalletMain.get());
    vpwallets.clear();
    bitdb.Flush(true);
    bitdb.Reset();
}

TestChain100Setup::TestChain100Setup() : WalletTestingSetup(CBaseChainParams::REGTEST)
{
  // CreateAndProcessBlock() does not support building SegWit blocks, so don't activate in these tests.
  // TODO: fix the code to support SegWit blocks.
  UpdateVersionBitsParameters(Consensus::DEPLOYMENT_SEGWIT, 0, Consensus::BIP9Deployment::NO_TIMEOUT);

  CUnitESecret vchSecret;
  bool fGood = vchSecret.SetString("cQTjnbHifWGuMhm9cRgQ23ip5KntTMfj3zwo6iQyxMVxSfJyptqL");
  assert(fGood);
  coinbaseKey = vchSecret.GetKey();
  {
    LOCK(pwalletMain->cs_wallet);
    assert(pwalletMain->AddKey(coinbaseKey));
  }

  WalletRescanReserver reserver(pwalletMain.get());
  reserver.reserve();
  pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);

  // Generate a 100-block chain:
  CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
  for (int i = 0; i < 100; i++)
  {
    if (i == 99) {
      GetComponent(Settings)->stake_split_threshold = 7600 * UNIT;
    }

    std::vector<CMutableTransaction> noTxns;
    CBlock b = CreateAndProcessBlock(noTxns, scriptPubKey);
    coinbaseTxns.push_back(*b.vtx[0]);
  }
  GetComponent(Settings)->stake_split_threshold = 0;
}

//
// Create a new block with just given transactions, coinbase paying to
// scriptPubKey, and try to add it to the current chain.
//
CBlock
TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns, const CScript& scriptPubKey, bool *processed)
{
  const CChainParams& chainparams = Params();
  std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey);
  CBlock& block = pblocktemplate->block;

  // Replace mempool-selected txns with just coinbase plus passed-in txns:
  block.vtx.resize(1);
  for (const CMutableTransaction& tx : txns)
    block.vtx.push_back(MakeTransactionRef(tx));
  // IncrementExtraNonce creates a valid coinbase and merkleRoot
  unsigned int extraNonce = 0;
  {
    LOCK(cs_main);
    IncrementExtraNonce(&block, chainActive.Tip(), extraNonce);
  }

  while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

  std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
  const bool was_processed = ProcessNewBlock(chainparams, shared_pblock, true, nullptr);
  assert(processed != nullptr || was_processed);
  if (processed != nullptr) {
    *processed = was_processed;
  }

  SyncWithValidationInterfaceQueue(); // To preven Wallet::ConnectBlock from running concurrently

  CBlock result = block;
  return result;
}

TestChain100Setup::~TestChain100Setup()
{
}
