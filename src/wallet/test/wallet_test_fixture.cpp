// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

#include <key_io.h>
#include <pow.h>
#include <rpc/server.h>
#include <wallet/db.h>
#include <wallet/rpcvalidator.h>
#include <consensus/merkle.h>

WalletTestingSetup::WalletTestingSetup(const std::string& chainName)
    : WalletTestingSetup([](Settings& s){}, chainName) {}

WalletTestingSetup::WalletTestingSetup(std::function<void(Settings&)> f, const std::string& chainName)
    : TestingSetup(chainName),
      m_wallet_ptr(new CWallet("mock", WalletDatabase::CreateMock())),
      m_wallet(*m_wallet_ptr)
{
    bool fFirstRun;

    f(settings);
    esperanza::WalletExtensionDeps deps;
    deps.settings = &settings;

    m_wallet.LoadWallet(fFirstRun);
    RegisterValidationInterface(&m_wallet);
    AddWallet(m_wallet_ptr);

    RegisterWalletRPCCommands(tableRPC);
    RegisterValidatorRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup()
{
    RemoveWallet(m_wallet_ptr);
    UnregisterValidationInterface(&m_wallet);
}

TestChain100Setup::TestChain100Setup() : WalletTestingSetup(CBaseChainParams::REGTEST)
{
  coinbaseKey = DecodeSecret("cQTjnbHifWGuMhm9cRgQ23ip5KntTMfj3zwo6iQyxMVxSfJyptqL");
  assert(coinbaseKey.IsValid());
  {
    LOCK(m_wallet.cs_wallet);
    assert(m_wallet.AddKey(coinbaseKey));
  }

  WalletRescanReserver reserver(&m_wallet);
  reserver.reserve();
  m_wallet.ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);

  // Generate a 100-block chain:
  GetComponent<Settings>()->stake_split_threshold = 0; // reset to 0
  CScript script_pubkey = GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID()));
  for (int i = 0; i < COINBASE_MATURITY; i++)
  {
    std::vector<CMutableTransaction> noTxns;
    CBlock b = CreateAndProcessBlock(noTxns, script_pubkey);
    m_coinbase_txns.push_back(*b.vtx[0]);
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
  std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(chainparams).CreateNewBlock(scriptPubKey, true /* fMineWitnessTx */);
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
  // Regenerate the merkle roots cause we possibly changed the txs included
  block.ComputeMerkleTrees();

  while (!CheckProofOfWork(block.GetHash(), block.nBits, chainparams.GetConsensus())) ++block.nNonce;

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
