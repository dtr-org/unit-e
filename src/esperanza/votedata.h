// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VOTEDATA_H
#define UNITE_ESPERANZA_VOTEDATA_H

#include <stdint.h>
#include <uint256.h>

namespace esperanza {

struct VoteData {

    uint256 m_validatorIndex;

    uint256 m_targetHash;

    uint32_t m_sourceEpoch;

    uint32_t m_targetEpoch;

    bool operator==(const VoteData& rhs) const
    {
        return this->m_validatorIndex == rhs.m_validatorIndex &&
               this->m_targetHash == rhs.m_targetHash &&
               this->m_sourceEpoch == rhs.m_sourceEpoch &&
               this->m_targetEpoch == rhs.m_targetEpoch;
    }
};

} // namespace esperanza

#endif // UNITE_ESPERANZA_VOTEDATA_H
