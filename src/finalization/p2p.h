// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_FINALIZATION_P2P
#define UNITE_FINALIZATION_P2P

#include <serialize.h>

/*
 * Implementation of UIP-21.
 */

class CNode;
class CNetMsgMaker;

namespace finalization {
namespace p2p {

//! \brief Represents anchors in blockchain used by node to request commits.
//!
//! CommitsLocator is used by get_commits message which requests commits in between
//! the most common block until "stop" block.
//!
//! "start" is a vector of block hashes. The first element must be last finalized
//! checkpoint. Other elements of vector is used to find most recent common hash.
//! "stop" is a hash of the stop block. 0x0 means to ignore it and process blocks
//! until next finalized checkpoint or tip reached.
struct CommitsLocator {
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

//! \brief Represents an element of "commits" message.
//!
//! "header" is a header of the block.
//! "commits" is a commits (aka esperanza transactions) contained in the block.
struct HeaderAndCommits {
  CBlockHeader header;
  std::vector<CTransactionRef> commits;

  HeaderAndCommits() = default;
  HeaderAndCommits(const CBlockHeader &header) : header(header) {}

  ADD_SERIALIZE_METHODS
  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(header);
    READWRITE(commits);
  }
};

//! \brief The "commits" message body.
//!
//! The response to "getcommits".
//!
//! "status" indicates the result of commits extraction:
//! 0 - stop or finalizdd checkpoint reached
//! 1 - tip of the main chain reached
//! 2 - message length exceeded
struct CommitsResponse {
  enum class Status : uint8_t {
    StopOrFinReached = 0,
    TipReached = 1,
    LengthExceeded = 2,
  };
  Status status = Status::StopOrFinReached;
  std::vector<HeaderAndCommits> data;

  ADD_SERIALIZE_METHODS
  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(*(uint8_t*)&status);
    READWRITE(data);
  }
};

//! \brief Process the "getcommits" message
//!
//! Collect commits in between the most recent common block hash and stop condition.
bool ProcessGetCommits(CNode *node, const CommitsLocator &locator, const CNetMsgMaker &msgMaker,
                       const CChainParams &params);

//! \brief Process the "commits" message
bool ProcessNewCommits(const CommitsResponse &commits, const CChainParams &chainparams,
                       CValidationState &validation_state, uint256 *failed_block_out);

} // p2p
} // finalization

#endif
