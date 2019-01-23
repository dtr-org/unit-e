// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <script/interpreter.h>
#include <serialize.h>
#include <tinyformat.h>
#include <usbdevice/ledger/dongleCommHidHidapi.h>
#include <usbdevice/ledgerapdu.h>
#include <usbdevice/ledgerdevice.h>
#include <util.h>

#include <hidapi/hidapi.h>

#include <cassert>
#include <string>
#include <vector>

#ifdef ENABLE_USBDEVICE

namespace usbdevice {

bool LedgerDevice::Open() {
  if (hid_init()) {
    return false;
  }

  if (!(handle = hid_open_path(m_path.c_str()))) {
    hid_exit();
    return false;
  }

  return true;
}

bool LedgerDevice::Close() {
  if (handle) {
    hid_close(handle);
  }
  handle = nullptr;

  hid_exit();
  return true;
}

bool LedgerDevice::SendAPDU(APDU &apdu, std::string &error) {
  int result, status_word;

  if (handle == nullptr && !Open()) {
    error = "Cannot open USB device";
    return false;
  }

  result = sendApduHidHidapi(handle, &apdu.m_in[0], apdu.size, &apdu.m_out[0],
                             apdu.m_out.size(), &status_word);

  if (result < 0) {
    error = "Error communicating with the device";
    return false;
  }

  if (status_word != SW_OK) {
    error = strprintf("Dongle application error: 0x%04x", status_word);
    return false;
  }

  apdu.m_out.resize(result);

  return true;
}

bool LedgerDevice::GetFirmwareVersion(std::string &firmware,
                                      std::string &error) {

  APDU apdu(BTCHIP_INS_GET_FIRMWARE_VERSION, 0x00, 0x00);
  if (!SendAPDU(apdu, error)) {
    return false;
  }

  if (apdu.m_out.size() < 5) {
    error = strprintf("Invalid read size: %d", apdu.m_out.size());
    return false;
  }

  firmware = strprintf("%s %d.%d.%d", (apdu.m_out[1] != 0 ? "Ledger" : ""),
                       apdu.m_out[2], apdu.m_out[3], apdu.m_out[4]);
  return true;
}

bool LedgerDevice::GetPubKey(const std::vector<uint32_t> &path, CPubKey &pk,
                             std::string &error) {
  CExtPubKey epk;
  if (!GetExtPubKey(path, epk, error)) {
    return false;
  }

  pk = epk.pubkey;
  return true;
}

static bool DecodeExtKey(std::vector<uint8_t> &buf, CExtPubKey &epk,
                         std::string &error) {
  // `buf` contains the pubkey size, the key itself, and the Base58
  // P2PKH address corresponding to it.
  uint8_t *response = &buf[0];
  size_t data_left = buf.size();

  if (data_left == 0) {
    return false;
  }
  uint8_t pk_size = *response++;
  data_left--;

  if (data_left < pk_size) {
    return false;
  }
  if (pk_size == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
    epk.pubkey.Set(response, response + pk_size);
  } else if (pk_size == CPubKey::PUBLIC_KEY_SIZE) {
    // Compress pubkey before returning
    if (response[0] != 0x04) {
      error = strprintf("Invalid public key starting with 0x%02x", response[0]);
      return false;
    }
    response[0] = (response[64] & 1) ? 0x03 : 0x02;
    epk.pubkey.Set(response, response + CPubKey::COMPRESSED_PUBLIC_KEY_SIZE);
  } else {
    error = strprintf("Invalid public key with length %d", pk_size);
    return false;
  }
  response += pk_size;
  data_left -= pk_size;

  CKeyID id = epk.pubkey.GetID();
  memcpy(&epk.vchFingerprint[0], &id, 4);

  if (data_left < 1) {
    return false;
  }
  uint8_t base58_size = *response++;
  data_left--;

  if (data_left < base58_size) {
    return false;
  }
  std::string base58_addr(response, response + base58_size);
  LogPrintf("Received pubkey for address %s\n", base58_addr.c_str());
  response += base58_size;
  data_left -= base58_size;

  if (data_left < 32) {
    return false;
  }
  epk.chaincode = ChainCode(response, 32);

  return true;
}

bool LedgerDevice::GetExtPubKey(const std::vector<uint32_t> &path,
                                CExtPubKey &epk, std::string &error) {
  APDU apdu;
  if (!GetExtPubKeyAPDU(path, apdu, error)) {
    return false;
  }

  if (!SendAPDU(apdu, error)) {
    return false;
  }

  if (!DecodeExtKey(apdu.m_out, epk, error)) {
    return false;
  }
  epk.nDepth = path.size();
  epk.nChild = (epk.nDepth > 0) ? path[epk.nDepth - 1] : 0;

  return true;
}

bool LedgerDevice::PrepareTransaction(const CTransaction &tx,
                                      const CCoinsViewCache &view,
                                      const CKeyStore &keystore, int hash_type,
                                      std::string &error) {
  std::vector<APDU> apdus;

  if (!GetPreparePhaseAPDUs(tx, view, apdus, error)) {
    return false;
  }

  for (auto &apdu : apdus) {
    if (!SendAPDU(apdu, error)) {
      return false;
    }
  }

  return true;
}

bool LedgerDevice::SignTransaction(const std::vector<uint32_t> &path,
                                   const CTransaction &tx, int n_in,
                                   const CScript &script_code, int hash_type,
                                   const CAmount &amount, SigVersion sigversion,
                                   std::vector<uint8_t> &signature,
                                   std::string &error) {
  std::vector<APDU> apdus;
  if (!GetSignPhaseAPDUs(path, tx, n_in, script_code, hash_type, amount,
                         sigversion, apdus, error)) {
    return false;
  }

  for (auto &apdu : apdus) {
    if (!SendAPDU(apdu, error)) {
      return false;
    }
  }

  auto &out = apdus[apdus.size() - 1].m_out;

  // 'out' now contains an ASN-1 encoded signature for the input,
  // plus one byte for the hash type
  assert(out[out.size() - 1] == hash_type);
  signature = std::move(out);

  // Drop the non-canonical parity bit
  signature[0] &= 0xFE;

  assert(CheckSignatureEncoding(signature, SCRIPT_VERIFY_DERSIG, nullptr));

  return true;
}

}  // namespace usbdevice

#endif
