// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/test/wallet_test_fixture.h>

#include <consensus/merkle.h>
#include <injector.h>
#include <key_io.h>
#include <rpc/server.h>
#include <test/test_unite_mocks.h>
#include <validation.h>
#include <wallet/db.h>
#include <wallet/rpcvalidator.h>
#include <wallet/rpcwalletext.h>
#include <boost/test/unit_test.hpp>

WalletTestingSetup::WalletTestingSetup(const std::string& chainName, UnitEInjectorConfiguration config)
    : WalletTestingSetup([](Settings& s){}, chainName, config) {}

WalletTestingSetup::WalletTestingSetup(
    std::function<void(Settings&)> f,
    const std::string& chainName,
    UnitEInjectorConfiguration config)
    : TestingSetup(chainName, config)
{
    bool fFirstRun;

    stake_validator_mock.mock_IsStakeMature.SetResult(true);

    f(settings);
    esperanza::WalletExtensionDeps deps(&settings, &stake_validator_mock,
                                        GetComponent<proposer::FinalizationRewardLogic>());

    m_wallet.reset(new CWallet("mock", WalletDatabase::CreateMock(), deps));
    m_wallet->LoadWallet(fFirstRun);
    RegisterValidationInterface(m_wallet.get());
    AddWallet(m_wallet);

    RegisterWalletRPCCommands(tableRPC);
    RegisterValidatorRPCCommands(tableRPC);
    RegisterWalletextRPCCommands(tableRPC);
}

WalletTestingSetup::~WalletTestingSetup()
{
    RemoveWallet(m_wallet);
    UnregisterValidationInterface(m_wallet.get());
}

TestChain100Setup::TestChain100Setup(UnitEInjectorConfiguration config)
    : WalletTestingSetup(CBaseChainParams::REGTEST, config),
      m_block_builder(proposer::BlockBuilder::New(&settings)),
      m_active_chain(staking::ActiveChain::New()),
      m_behavior(blockchain::Behavior::NewForNetwork(blockchain::Network::regtest))
{

  coinbaseKey = DecodeSecret("cQTjnbHifWGuMhm9cRgQ23ip5KntTMfj3zwo6iQyxMVxSfJyptqL");
  assert(coinbaseKey.IsValid());
  {
    LOCK(m_wallet->cs_wallet);
    assert(m_wallet->AddKey(coinbaseKey));
  }

  WalletRescanReserver reserver(m_wallet.get());
  reserver.reserve();
  m_wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);

  // Generate a 100-block chain:
  GetComponent<Settings>()->stake_split_threshold = 0; // do not split stake
  CScript script_pubkey = GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID()));
  for (int i = 0; i < COINBASE_MATURITY; i++)
  {
    std::vector<CMutableTransaction> noTxns;
    CBlock b = CreateAndProcessBlock(noTxns, script_pubkey);
    m_coinbase_txns.push_back(*b.vtx[0]);
    auto x = b.GetHash().ToString();
    assert(!x.empty());
  }
}

std::shared_ptr<const CBlock> TestChain100Setup::CreateBlock(
    const std::vector<CMutableTransaction>& txns,
    const CScript& coinbase_script,
    boost::optional<staking::Coin> stake)
{
  esperanza::WalletExtension &wallet_ext = m_wallet->GetWalletExtension();

  if (!stake) {
    LOCK2(m_active_chain->GetLock(), wallet_ext.GetLock());
    staking::CoinSet coin_set = wallet_ext.GetStakeableCoins();
    BOOST_REQUIRE(!coin_set.empty());
    stake.emplace(*coin_set.begin());
  }

  const CAmount fees = 0;
  uint256 snapshot_hash;
  {
    LOCK(m_active_chain->GetLock());
    snapshot_hash = m_active_chain->ComputeSnapshotHash();
  }

  std::vector<CTransactionRef> tx_refs;
  for (const CMutableTransaction& tx : txns) {
    tx_refs.push_back(MakeTransactionRef(tx));
  }

  const CBlockIndex *tip = m_active_chain->GetTip();
  blockchain::Height tip_height = tip->nHeight;

  proposer::EligibleCoin coin = {
      *stake,
      uint256(),
      m_behavior->CalculateBlockReward(tip_height),
      static_cast<blockchain::Height>(tip_height+1),
      static_cast<blockchain::Time>(std::max(tip->GetMedianTimePast()+1, GetTime())),
      tip->nBits
  };

  std::shared_ptr<const CBlock> block = m_block_builder->BuildBlock(
      *tip,
      snapshot_hash,
      coin,
      {},
      tx_refs,
      fees,
      GetComponent<proposer::FinalizationRewardLogic>()->GetFinalizationRewards(*tip),
      coinbase_script,
      wallet_ext);

  return block;
}

bool TestChain100Setup::ProcessBlock(const std::shared_ptr<const CBlock> block) {
  const bool was_processed = ProcessNewBlock(Params(), block, true, nullptr);

  SyncWithValidationInterfaceQueue(); // To prevent Wallet::ConnectBlock from running concurrently

  return was_processed;
}

CBlock TestChain100Setup::CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns,
                                                const CScript& coinbase_script,
                                                const boost::optional<staking::Coin> stake,
                                                bool *processed) {
  std::shared_ptr<const CBlock> block = CreateBlock(txns, coinbase_script, stake);

  const bool was_processed = ProcessBlock(block);
  BOOST_REQUIRE(processed != nullptr || was_processed);
  if (processed) {
    *processed = was_processed;
  }

  return *block;
}

TestChain100Setup::~TestChain100Setup()
{
}
