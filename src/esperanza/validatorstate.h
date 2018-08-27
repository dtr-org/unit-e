// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATORSTATE_H
#define UNITE_ESPERANZA_VALIDATORSTATE_H

#include <map>
#include <esperanza/votedata.h>
#include <uint256.h>
#include <primitives/transaction.h>
#include <wallet/wallet.h>

namespace esperanza {

struct ValidatorState {

    enum class ValidatorPhase {
        NOT_VALIDATING,
        IS_VALIDATING,
        WAITING_DEPOSIT_CONFIRMATION,
        WAITING_DEPOSIT_FINALIZATION,
    };

    ValidatorState() :
            m_validatorIndex(),
            m_lastVotableTx(nullptr),
            m_voteMap(),
            m_lastSourceEpoch(0),
            m_lastTargetEpoch(0),
            m_depositEpoch(std::numeric_limits<uint32_t >::max()),
            m_endDynasty(std::numeric_limits<uint32_t >::max()),
            m_startDynasty(std::numeric_limits<uint32_t >::max()) {}

    ValidatorPhase m_phase = ValidatorPhase::NOT_VALIDATING;
    uint256 m_validatorIndex;
    CTransactionRef m_lastVotableTx;
    uint32_t m_depositEpoch;

    std::map<uint32_t, VoteData> m_voteMap;
    uint32_t m_lastSourceEpoch;
    uint32_t m_lastTargetEpoch;
    uint32_t m_endDynasty; //UNIT-E: To set when the logout happens
    uint32_t m_startDynasty;
};

}

#endif // UNITE_ESPERANZA_VALIDATORSTATE_H
