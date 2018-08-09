// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_WALLET_RPCMNEMONIC_H
#define UNIT_E_WALLET_RPCMNEMONIC_H

#include <rpc/server.h>

/*! \brief The mnemonic commands allow for creating private extended keys according to BIP39.
 *
 * These command do not require the wallet to be unlocked (or any available in fact).
 */
void RegisterMnemonicRPCCommands(CRPCTable &t);

#endif //UNIT_E_WALLET_RPCMNEMONIC_H
