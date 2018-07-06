// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_VALIDATORSTATE_H
#define UNIT_E_VALIDATORSTATE_H

#include <stdint.h>

enum class ValidatorState : uint8_t
{
    NOT_VALIDATING                  = 0,
    IS_VALIDATING                   = 1,
    NOT_VALIDATING_LOCKED           = 2,
    NOT_VALIDATING_BALANCE          = 3,
    WAITING_DEPOSIT_CONFIRMATION    = 4,
};

#endif //UNIT_E_VALIDATORSTATE_H
