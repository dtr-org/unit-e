// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_WALLETINITINTERFACE_H
#define UNITE_WALLETINITINTERFACE_H

#include <string>

class CScheduler;
class CRPCTable;

namespace esperanza {
class WalletExtensionDeps;
}

class WalletInitInterface {
public:
    /** Get wallet help string */
    virtual void AddWalletOptions() const = 0;
    /** Check wallet parameter interaction */
    virtual bool ParameterInteraction() const = 0;
    /** Register wallet RPC*/
    virtual void RegisterRPC(CRPCTable &) const = 0;
    /** Verify wallets */
    virtual bool Verify() const = 0;
    /** Open wallets*/
    virtual bool Open(const esperanza::WalletExtensionDeps& ) const = 0;
    /** Start wallets*/
    virtual void Start(CScheduler& scheduler) const = 0;
    /** Flush Wallets*/
    virtual void Flush() const = 0;
    /** Stop Wallets*/
    virtual void Stop() const = 0;
    /** Close wallets */
    virtual void Close() const = 0;

    virtual ~WalletInitInterface() {}
};

#endif // UNITE_WALLETINITINTERFACE_H
