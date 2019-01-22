// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_USBDEVICE_USBDEVICE_H
#define UNITE_USBDEVICE_USBDEVICE_H

#include <key.h>
#include <keystore.h>
#include <script/sign.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

class UniValue;
class CCoinsViewCache;

namespace usbdevice {

enum DeviceTypeID {
  USBDEVICE_ANY = -1,
  USBDEVICE_NONE = 0,
  USBDEVICE_DEBUG = 1,
  USBDEVICE_SIZE,
};

class DeviceType {
 public:
  DeviceType(
      int vendor_id, int product_id,
      const char *vendor, const char *product,
      DeviceTypeID type_id)
      : m_vendor_id(vendor_id), m_product_id(product_id), m_vendor(vendor), m_product(product), m_type_id(type_id){};

  const int m_vendor_id;
  const int m_product_id;
  const std::string m_vendor;
  const std::string m_product;
  const DeviceTypeID m_type_id;
};

extern const DeviceType usbDeviceTypes[];

class USBDevice {
 public:
  USBDevice(const DeviceType *type, const char *path, const char *serial_no, int interface)
      : m_type(type), m_path(path), m_serial_no(serial_no), m_interface(interface){};

  virtual ~USBDevice(){};

  virtual bool Open() = 0;
  virtual bool Close() = 0;

  virtual bool GetFirmwareVersion(std::string &firmware, std::string &error) = 0;

  virtual bool GetPubKey(
      const std::vector<uint32_t> &path, CPubKey &pk, std::string &error) = 0;
  virtual bool GetExtPubKey(
      const std::vector<uint32_t> &path, CExtPubKey &ekp,
      std::string &error) = 0;

  virtual bool PrepareTransaction(
      const CTransaction &tx, const CCoinsViewCache &view,
      const CKeyStore &keystore, int hash_type, std::string &error) = 0;

  virtual bool SignTransaction(
      const std::vector<uint32_t> &path,
      const std::vector<uint8_t> &shared_secret,
      const CTransaction &tx,
      int n_in, const CScript &script_code, int hash_type,
      const CAmount &amount, SigVersion sigversion,
      std::vector<uint8_t> &signature, std::string &error) = 0;

  const DeviceType *m_type = nullptr;
  const std::string m_path;
  const std::string m_serial_no;
  const int m_interface;
  std::string m_error;
};

class DeviceSignatureCreator : public BaseSignatureCreator {
  const CTransaction &m_tx;
  unsigned int m_nin;
  int m_hash_type;
  CAmount m_amount;
  std::shared_ptr<USBDevice> m_device;
  const TransactionSignatureChecker m_checker;

 public:
  DeviceSignatureCreator(
      std::shared_ptr<USBDevice> device, const CTransaction &tx,
      unsigned int nin, const CAmount &amount, int hash_type = SIGHASH_ALL);
  const BaseSignatureChecker &Checker() const override { return m_checker; }
  bool CreateSig(
      const SigningProvider &provider,
      std::vector<unsigned char> &signature, const CKeyID &keyid,
      const CScript &script_code, SigVersion sigversion) const override;
};

typedef std::vector<std::shared_ptr<USBDevice>> DeviceList;

bool ListAllDevices(DeviceList &devices);

std::shared_ptr<USBDevice> SelectDevice(std::string &error);

}  // namespace usbdevice

#endif  // UNITE_USBDEVICE_USBDEVICE_H
