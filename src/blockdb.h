// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKDB_H
#define UNIT_E_BLOCKDB_H

#include <chain.h>
#include <primitives/block.h>

#include <boost/optional.hpp>

#include <memory>

//! \brief An interface to block read/write operations.
class BlockDB {

 public:
  //! \brief This function reads a block from the database give a CBlockIndex
  //! reference to it.
  //!
  //! \param index the reference to the block to read.
  //! \return the block if found.
  //!
  virtual boost::optional<CBlock> ReadBlock(const CBlockIndex &index) = 0;

  virtual ~BlockDB() = default;

  //! \brief Factory method for creating a BlockDB.
  static std::unique_ptr<BlockDB> New();
};

#endif  //UNIT_E_BLOCKDB_H
