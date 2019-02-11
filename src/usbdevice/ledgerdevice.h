// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_LEDGERDEVICE_H_INCLUDED
#define UNITE_LEDGERDEVICE_H_INCLUDED

#include <usbdevice/usbdevice.h>

#include <string>
#include <vector>

struct hid_device_;
typedef struct hid_device_ hid_device;

struct APDU;

namespace usbdevice {

class LedgerDevice : public USBDevice {
 protected:
  hid_device *handle = nullptr;

 public:
  LedgerDevice(const DeviceType &type, const char *path, const char *serial_no,
               int interface)
      : USBDevice(type, path, serial_no, interface) {}

  virtual ~LedgerDevice() { Close(); }

  bool Open() override;
  bool Close() override;

  bool GetFirmwareVersion(std::string &firmware, std::string &error) override;

  bool GetPubKey(const std::vector<uint32_t> &path, CPubKey &pk,
                 std::string &error) override;
  bool GetExtPubKey(const std::vector<uint32_t> &path, CExtPubKey &ekp,
                    std::string &error) override;

  bool PrepareTransaction(const CTransaction &tx, const CCoinsViewCache &view,
                          const CKeyStore &keystore, int hash_type,
                          std::string &error) override;

  bool SignTransaction(const std::vector<uint32_t> &path,
                       const CTransaction &tx, int n_in,
                       const CScript &script_code, int hash_type,
                       const CAmount &amount, SigVersion sigversion,
                       std::vector<uint8_t> &signature,
                       std::string &error) override;

 private:
  bool SendAPDU(APDU &apdu, std::string &error);
};

}  // namespace usbdevice

#endif  // UNITE_LEDGERDEVICE_H_INCLUDED
