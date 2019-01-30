// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <tinyformat.h>
#include <usbdevice/ledgerapdu.h>

#include <algorithm>

namespace usbdevice {

template <typename T>
inline constexpr uint8_t u8(T arg) {
  return static_cast<uint8_t>(arg);
}

//! Generate a command APDU for retrieving an HD hardware wallet's
//! public key, for a given derivation path.
bool GetExtPubKeyAPDU(const std::vector<uint32_t> &path, APDU &apdu_out,
                      std::string &error) {
  if (path.size() > MAX_BIP32_PATH) {
    error = strprintf("%s: BIP32 path too long.", __func__);
    return false;
  }

  APDU apdu(BTCHIP_INS_GET_WALLET_PUBLIC_KEY, 0x00, 0x00);
  apdu << u8(path.size());
  for (uint32_t child : path) {
    apdu.write_be(child);
  }
  apdu_out = std::move(apdu);

  return true;
}

//! \brief Generate command APDUs for initializing a wallet's transaction state
//! and prepare it for signing.
//!
//! See https://ledgerhq.github.io/btchip-doc/bitcoin-technical-beta.html for
//! description of the HASH INPUT START command and signing process.
//!
//! \param[in] tx the transaction to be signed
//! \param[in] view a cache of spendable coins in the wallet
//! \param[out] apdus_out the list of commands to send to the device
bool GetPreparePhaseAPDUs(const CTransaction &tx, const CCoinsViewCache &view,
                          std::vector<APDU> &apdus_out, std::string &error) {
  apdus_out.clear();

  {
    // The transaction is serialized over several APDUs
    APDU apdu(BTCHIP_INS_HASH_INPUT_START, 0x00, 0x02);
    apdu << tx.nVersion;
    ::WriteCompactSize(apdu, tx.vin.size());
    apdus_out.emplace_back(std::move(apdu));
  }

  // Serializing inputs
  for (const auto &txin : tx.vin) {
    if (!view.HaveCoin(txin.prevout)) {
      error = "Transaction input is invalid or already spent";
      return false;
    }

    const Coin &coin = view.AccessCoin(txin.prevout);
    APDU apdu(BTCHIP_INS_HASH_INPUT_START, 0x80, 0x00);
    apdu << u8(0x02)  // Indicates a SegWit input
         << txin.prevout
         << coin.out.nValue
         << u8(0x00)  // In the pre-sign phase, scriptSig is empty
         << txin.nSequence;
    apdus_out.emplace_back(std::move(apdu));
  }

  {
    APDU apdu(BTCHIP_INS_HASH_INPUT_FINALIZE_FULL, 0x00, 0x00);
    ::WriteCompactSize(apdu, tx.vout.size());

    // Serializing outputs
    for (const auto &txout : tx.vout) {
      apdu << txout.nValue;
      ::WriteCompactSize(apdu, txout.scriptPubKey.size());

      // Large scriptPubKeys are split over several APDUs
      size_t script_data_left = txout.scriptPubKey.size();
      auto it = txout.scriptPubKey.begin();

      while (script_data_left > 0) {
        size_t space_left = apdu.max_size - apdu.size;
        size_t chunk_size = std::min(space_left, script_data_left);

        apdu.write(it, chunk_size);
        script_data_left -= chunk_size;
        it += chunk_size;

        apdus_out.emplace_back(std::move(apdu));
        apdu = std::move(APDU(BTCHIP_INS_HASH_INPUT_FINALIZE_FULL, 0x00, 0x00));
      }
    }

    // Mark last block as such
    apdus_out.rbegin()->m_in[2] = 0x80;
  }

  return true;
}

//! \brief Generate command APDUs for initializing a wallet's transaction state
//! and prepare it for signing.
//!
//! \param[in] path the BIP32 derivation path for the signing key
//! \param[in] tx the transaction to be signed
//! \param[in] n_in the input number to be signed
//! \param[in] script_code the previous output's scriptPubKey
//! \param[in] amount the monetary value of the previous output
//! \param[out] apdus_out the list of commands to send to the device
bool GetSignPhaseAPDUs(const std::vector<uint32_t> &path,
                       const CTransaction &tx, int n_in,
                       const CScript &script_code, int hash_type,
                       const CAmount &amount, SigVersion sigversion,
                       std::vector<APDU> &apdus_out, std::string &error) {
  apdus_out.clear();

  if (path.size() > MAX_BIP32_PATH) {
    error = strprintf("%s: BIP32 path too long.", __func__);
    return false;
  }

  {
    // To get the signature, we send a pseudo-transaction with 1 input and no outputs
    APDU apdu(BTCHIP_INS_HASH_INPUT_START, 0x00, 0x80);
    apdu << tx.nVersion
         << u8(1);
    apdus_out.emplace_back(std::move(apdu));
  }

  {
    const auto &txin = tx.vin[n_in];
    APDU apdu(BTCHIP_INS_HASH_INPUT_START, 0x80, 0x00);
    apdu << u8(0x02)  // This is a SegWit input
         << txin.prevout
         << amount;
    ::WriteCompactSize(apdu, script_code.size());

    // Split a large scriptPubKey over several APDUs
    size_t script_data_left = script_code.size();
    auto it = script_code.begin();

    while (script_data_left > 0) {
      if (apdu.max_size == apdu.size) {
        apdus_out.emplace_back(std::move(apdu));
        apdu = std::move(APDU(BTCHIP_INS_HASH_INPUT_START, 0x80, 0x00));
      }

      size_t space_left = apdu.max_size - apdu.size;
      size_t chunk_size = std::min(space_left, script_data_left);
      apdu.write(it, chunk_size);
      script_data_left -= chunk_size;
      it += chunk_size;
    }

    if (apdu.max_size - apdu.size < 4) {
      apdus_out.emplace_back(std::move(apdu));
      apdu = std::move(APDU(BTCHIP_INS_HASH_INPUT_START, 0x80, 0x00));
    }

    apdu << txin.nSequence;
    apdus_out.emplace_back(std::move(apdu));
  }

  {
    // Sign the generated hash
    APDU apdu(BTCHIP_INS_HASH_SIGN, 0x00, 0x00);

    // The BIP32 derivation path for the signing key
    apdu << u8(path.size());
    for (uint32_t child : path) {
      apdu.write_be(child);
    }

    // The key is not protected by a PIN
    apdu << u8(0x00);

    apdu.write_be(tx.nLockTime);
    apdu << u8(hash_type);
    apdus_out.emplace_back(std::move(apdu));
  }

  return true;
}

}  // namespace usbdevice
