// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_USBDEVICE_DEBUGDEVICE_H
#define UNITE_USBDEVICE_DEBUGDEVICE_H

#include <key.h>
#include <usbdevice/usbdevice.h>

#include <memory>

namespace usbdevice {

class DebugDevice : public USBDevice {
 private:
  CExtKey m_ekv;

 public:
  DebugDevice();

  bool Open() override;

  bool Close() override;

  bool GetFirmwareVersion(std::string &firmware, std::string &error) override;

  bool GetPubKey(const std::vector<uint32_t> &path, CPubKey &pk, std::string &error) override;

  bool GetExtPubKey(const std::vector<uint32_t> &path, CExtPubKey &ekp, std::string &error) override;

  bool PrepareTransaction(
      const CTransaction &tx, const CCoinsViewCache &view,
      const CKeyStore &keystore, int hash_type, std::string &error) override;

  bool SignTransaction(
      const std::vector<uint32_t> &path, const CTransaction &tx, int n_in,
      const CScript &script_code, int hash_type, const CAmount &amount,
      SigVersion sigversion, std::vector<uint8_t> &signature,
      std::string &error) override;
};

}  // namespace usbdevice

#endif  // UNITE_USBDEVICE_DEBUGDEVICE_H
