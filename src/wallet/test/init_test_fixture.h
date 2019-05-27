// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_WALLET_TEST_INIT_TEST_FIXTURE_H
#define UNITE_WALLET_TEST_INIT_TEST_FIXTURE_H

#include <interfaces/chain.h>
#include <test/test_unite.h>


struct InitWalletDirTestingSetup: public BasicTestingSetup {
    explicit InitWalletDirTestingSetup(const std::string& chainName = CBaseChainParams::REGTEST);
    ~InitWalletDirTestingSetup();
    void SetWalletDir(const fs::path& walletdir_path);

    fs::path m_datadir;
    fs::path m_cwd;
    std::map<std::string, fs::path> m_walletdir_path_cases;
    std::unique_ptr<interfaces::Chain> m_chain = interfaces::MakeChain();
    std::unique_ptr<interfaces::ChainClient> m_chain_client;
};

#endif // UNITE_WALLET_TEST_INIT_TEST_FIXTURE_H
