#ifndef UNITE_EXTKEY_H_INCLUDED
#define UNITE_EXTKEY_H_INCLUDED

#include <key.h>
#include <utilstrencodings.h>

#include <cstdint>
#include <string>
#include <vector>


const uint32_t BIP32_HARDENED_KEY_LIMIT = 0x80000000;

//! \brief Return the default BIP44 account path for this coin
std::string GetDefaultAccountPathString();

//! \brief Transform a BIP32 path string into a vector of child offsets
bool ParseExtKeyPath(const std::string &path_string, std::vector<uint32_t> &path, std::string &error);

//! \brief Transform a a vector of BIP32 child offsets into a path string
std::string FormatExtKeyPath(const std::vector<uint32_t> &path);

//! \brief Display an extended pubkey as a hex string
std::string ExtKeyToString(const CExtPubKey &epk);

#endif  // UNITE_EXTKEY_H_INCLUDED
