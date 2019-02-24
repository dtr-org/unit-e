// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_BLOCKCHAIN_CUSTOM_H
#define UNIT_E_BLOCKCHAIN_CUSTOM_H

#include <blockchain/blockchain_parameters.h>

#include <univalue/include/univalue.h>
#include <boost/optional.hpp>

#include <string>

namespace blockchain {

//! \brief Read blockchain::Parameters from a UniValue json object.
boost::optional<blockchain::Parameters> ReadCustomParametersFromJson(
    const UniValue &json,
    const blockchain::Parameters &base_parameters  //!< base params to take values from
);

//! \brief Read blockchain::Parameters from a UniValue json object.
//!
//! Same as the 2-argument version but takes an error reporting function
//! which will be called whenever an invalid value is found.
boost::optional<blockchain::Parameters> ReadCustomParametersFromJson(
    const UniValue &json,
    const blockchain::Parameters &base_parameters,  //!< base params to take values from
    const std::function<void(const std::string &)> &report_error);

//! \brief Read blockchain::Parameters from a JSON String.
boost::optional<blockchain::Parameters> ReadCustomParametersFromJsonString(
    const std::string &json_string,
    const blockchain::Parameters &base_parameters  //!< base params to take values from
);

//! \brief Read blockchain::Parameters from a JSON String.
//!
//! Same as the 2-argument version but takes an error reporting function
//! which will be called whenever an invalid value is found.
boost::optional<blockchain::Parameters> ReadCustomParametersFromJsonString(
    const std::string &json_string,
    const blockchain::Parameters &base_parameters,  //!< base params to take values from
    const std::function<void(const std::string &)> &report_error);

}  // namespace blockchain

#endif  //UNIT_E_BLOCKCHAIN_CUSTOM_H
