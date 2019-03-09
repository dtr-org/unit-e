// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_FINALIZER_COMMITS_TYPES
#define UNITE_P2P_FINALIZER_COMMITS_TYPES

#include <primitives/block.h>
#include <serialize.h>

namespace p2p {

//! \brief Represents anchors in blockchain used by node to request commits.
//!
//! FinalizerCommitsLocator is used by getcommits message which requests commits
//! in between the most common block until "stop" block.
//!
//! "start" is a vector of block hashes. The first element must be last finalized
//! checkpoint. Other elements of vector is used to find most recent common hash.
//! "stop" is a hash of the stop block. 0x0 means to ignore it and process blocks
//! until next finalized checkpoint or tip reached.
struct FinalizerCommitsLocator {
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

//! \brief A combination of block header and finalizer commits, but not a full block.
struct HeaderAndFinalizerCommits {
  //! \brief The header part of the block.
  CBlockHeader header;

  //! \brief The finalizer commits.
  std::vector<CTransactionRef> commits;

  HeaderAndFinalizerCommits() = default;
  HeaderAndFinalizerCommits(const CBlockHeader &header) : header(header) {}

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
//! 0 - stop or finalized checkpoint reached
//! 1 - tip of the main chain reached
//! 2 - message length exceeded
struct FinalizerCommitsResponse {
  enum class Status : uint8_t {
    StopOrFinalizationReached = 0,
    TipReached = 1,
    LengthExceeded = 2,
  };
  Status status = Status::StopOrFinalizationReached;
  std::vector<HeaderAndFinalizerCommits> data;

  ADD_SERIALIZE_METHODS
  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(*reinterpret_cast<uint8_t *>(&status));
    READWRITE(data);
  }
};

}  // namespace p2p

#endif
