// This file was automatically generated. Please do not modify it.

#ifndef UNIT_E_BLOCKCHAIN_MAINNET_FUNDS_H
#define UNIT_E_BLOCKCHAIN_MAINNET_FUNDS_H

#include <blockchain/blockchain_genesis.h>

namespace blockchain {

Funds MainnetFunds() {
    return Funds{
        P2WPKH(10000 * UNIT, "33a471b2c4d3f45b9ab4707455f7d2e917af5a6e"),
        P2WPKH(10000 * UNIT, "7eac29a2e24c161e2d18d8d1249a6327d18d390f"),
        P2WPKH(10000 * UNIT, "caca901140bf287eff2af36edeb48503cec4eb9f"),
        P2WPKH(10000 * UNIT, "1f34ea7e96d82102b22afed6d53d02715f8f6621"),
        P2WPKH(10000 * UNIT, "eb07ad5db790ee4324b5cdd635709f47e41fd867"),
    };
}

} // namespace blockchain

#endif
