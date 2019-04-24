// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

#include <injector.h>
#include <rpc/server.h>
#include <validation.h>
#include <wallet/db.h>
#include <wallet/rpcvalidator.h>
#include <wallet/rpcwalletext.h>
#include <consensus/merkle.h>
#include <test/test_unite_mocks.h>

WalletTestingSetup::WalletTestingSetup(const std::string& chainName, UnitEInjectorConfiguration config)
    : WalletTestingSetup([](Settings& s){}, chainName, config) {}

WalletTestingSetup::WalletTestingSetup(
    std::function<void(Settings&)> f,
    const std::string& chainName,
    UnitEInjectorConfiguration config)
    : TestingSetup(chainName, config)
{
    bitdb.MakeMock();

    bool fFirstRun;
    g_address_type = OUTPUT_TYPE_DEFAULT;
    g_change_type = OUTPUT_TYPE_DEFAULT;
    std::unique_ptr<CWalletDBWrapper> dbw(new CWalletDBWrapper(&bitdb, "wallet_test.dat"));

    f(settings);
    esperanza::WalletExtensionDeps deps(&settings, &stake_validator_mock);

    pwalletMain = MakeUnique<CWallet>(deps, std::move(dbw));
    pwalletMain->LoadWallet(fFirstRun);
    vpwallets.insert(vpwallets.begin(), &*pwalletMain);
    RegisterValidationInterface(pwalletMain.get());

    RegisterWalletRPCCommands(tableRPC);
    RegisterValidatorRPCCommands(tableRPC);
    RegisterWalletextRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup()
{
    UnregisterValidationInterface(pwalletMain.get());
    vpwallets.clear();
    bitdb.Flush(true);
    bitdb.Reset();
}

TestChain100Setup::TestChain100Setup(UnitEInjectorConfiguration config)
    : WalletTestingSetup(CBaseChainParams::REGTEST, config)
{
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
  GetComponent<Settings>()->stake_split_threshold = 0; // reset to 0
  CScript script_pubkey = GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID()));
  for (int i = 0; i < COINBASE_MATURITY; i++)
  {
    std::vector<CMutableTransaction> noTxns;
    CBlock b = CreateAndProcessBlock(noTxns, script_pubkey);
    coinbaseTxns.push_back(*b.vtx[0]);
    auto x = b.GetHash().ToString();
    assert(!x.empty());
  }
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
  for (const CMutableTransaction& tx : txns) {
    block.vtx.push_back(MakeTransactionRef(tx));
  }

  // Regenerate the merkle roots cause we possibly changed the txs included
  block.ComputeMerkleTrees();

  std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(block);
  const bool was_processed = ProcessNewBlock(chainparams, shared_pblock, true, nullptr);
  assert(processed != nullptr || was_processed);
  if (processed != nullptr) {
    *processed = was_processed;
  }

  SyncWithValidationInterfaceQueue(); // To prevent Wallet::ConnectBlock from running concurrently

  CBlock result = block;
  return result;
}

TestChain100Setup::~TestChain100Setup()
{
}
