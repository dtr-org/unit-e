// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_CUSTOM_PARAMETERS_H
#define UNIT_E_BLOCKCHAIN_CUSTOM_PARAMETERS_H

#include <blockchain/blockchain_parameters.h>

#include <univalue/include/univalue.h>
#include <boost/optional.hpp>

#include <string>

namespace blockchain {

class FailedToParseCustomParametersError : public std::runtime_error {

 public:
  explicit FailedToParseCustomParametersError(std::string&& what) : runtime_error(what) {}
};

//! \brief Read blockchain::Parameters from a UniValue json object.
blockchain::Parameters ReadCustomParametersFromJson(
    const UniValue &json,
    const blockchain::Parameters &base_parameters  //!< base params to take values from
);

//! \brief Read blockchain::Parameters from a JSON String.
blockchain::Parameters ReadCustomParametersFromJsonString(
    const std::string &json_string,
    const blockchain::Parameters &base_parameters  //!< base params to take values from
);

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_CUSTOM_PARAMETERS_H
