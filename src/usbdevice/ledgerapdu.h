// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_USBDEVICE_LEDGERAPDU_H_INCLUDED
#define UNITE_USBDEVICE_LEDGERAPDU_H_INCLUDED

#include <amount.h>
#include <script/interpreter.h>
#include <serialize.h>
#include <usbdevice/ledger/btchipApdu.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

class CCoinsViewCache;
class CKeyStore;
class CScript;
class CTransaction;

namespace usbdevice {

constexpr size_t IO_APDU_BUFFER_SIZE = 260;

//! Represents an APDU which carries its input and output byte vectors with it.
//! Also acts as a Stream for serialization operations.
struct APDU {
  std::vector<uint8_t> m_in;
  std::vector<uint8_t> m_out;
  size_t size;
  size_t max_size;

  APDU() : APDU(0, 0, 0) {}

  APDU(uint8_t ins, uint8_t p1, uint8_t p2)
      : m_in(IO_APDU_BUFFER_SIZE),
        m_out(IO_APDU_BUFFER_SIZE),
        size(5),
        max_size(IO_APDU_BUFFER_SIZE) {
    m_in[0] = BTCHIP_CLA;
    m_in[1] = ins;
    m_in[2] = p1;
    m_in[3] = p2;
    m_in[4] = 0;
  }

  APDU(APDU &&other)
      : m_in(std::move(other.m_in)), m_out(std::move(other.m_out)) {
    size = other.size;
    max_size = other.max_size;
  }

  APDU &operator=(APDU &&other) {
    m_in = std::move(other.m_in);
    m_out = std::move(other.m_out);
    size = other.size;
    max_size = other.max_size;
    return *this;
  }

  template <typename InputIt>
  void write(InputIt data, size_t n) {
    // The caller must ensure that enough space is available
    assert(max_size - size >= n);

    std::copy(data, data + n, m_in.begin() + size);
    size += n;
    m_in[4] = static_cast<uint8_t>(size - 5);
  }

  void write_be(uint32_t n) {
    n = htobe32(n);
    write(reinterpret_cast<char *>(&n), sizeof(n));
  }

  template <typename T>
  APDU &operator<<(const T &obj) {
    ::Serialize(*this, obj);
    return (*this);
  }
};

//! Generate a command APDU for retrieving an HD hardware wallet's
//! public key.
//!
//! \param[in] path the BIP32 derivation path for the public key
bool GetExtPubKeyAPDU(const std::vector<uint32_t> &path, APDU &apdu_out,
                      std::string &error);

//! \brief Generate command APDUs for initializing a wallet's transaction state
//! and prepare it for signing.
//!
//! See https://ledgerhq.github.io/btchip-doc/unite-technical-beta.html for
//! description of the HASH INPUT START command and signing process.
//!
//! \param[in] tx the transaction to be signed
//! \param[in] view a cache of spendable coins in the wallet
//! \param[out] apdus_out the list of commands to send to the device
bool GetPreparePhaseAPDUs(const CTransaction &tx, const CCoinsViewCache &view,
                          std::vector<APDU> &apdus_out, std::string &error);

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
                       std::vector<APDU> &apdus_out, std::string &error);

}  // namespace usbdevice

#endif  // UNITE_USBDEVICE_LEDGERAPDU_H_INCLUDED
