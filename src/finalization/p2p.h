// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_P2P
#define UNITE_FINALIZATION_P2P

#include <serialize.h>

class CNode;
class CNetMsgMaker;

namespace finalization {
namespace p2p {

struct Locator {
  std::vector<uint256> start;
  uint256 stop;

  ADD_SERIALIZE_METHODS
  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(start);
    READWRITE(stop);
  }

  std::string ToString() const;
};

struct HeaderAndCommits {
  CBlockHeader header;
  std::vector<CTransactionRef> commits;

  HeaderAndCommits() = default;
  HeaderAndCommits(CBlockHeader const &header) : header(header) {}

  ADD_SERIALIZE_METHODS
  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(header);
    READWRITE(commits);
  }
};

struct CommitsResponse {
  enum Status {
    StopOrFinReached = 0,
    TipReached = 1,
    LengthExceeded = 2,
  };
  uint8_t status = Status::StopOrFinReached;
  std::vector<HeaderAndCommits> data;

  ADD_SERIALIZE_METHODS
  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(status);
    READWRITE(data);
  }
};

bool ProcessGetCommits(CNode *node, Locator const &locator, CNetMsgMaker const &msgMaker,
                       CChainParams const &params);

bool ProcessNewCommits(CommitsResponse const &commits, CChainParams const &chainparams);

} // p2p
} // finalization

namespace util {
inline std::string to_string(finalization::p2p::Locator const &locator) {
  return locator.ToString();
}
} // util

#endif
