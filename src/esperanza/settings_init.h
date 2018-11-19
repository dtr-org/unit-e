// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_ESPERANZA_SETTINGS_INIT_H
#define UNIT_E_ESPERANZA_SETTINGS_INIT_H

#include <esperanza/settings.h>
#include <util.h>

namespace esperanza {

//! \brief Initializes Esperanza settings from command line args.
//!
//! This function can only ever be invoked once over the lifetime
//! of the application - during initialization. The idea is that every component
//! receives a reference to one settings object which is not globally available
//! in its initialization. This facilitates isolation of components (due to the
//! elimination of global state) and automatically enhances testability of
//! individual units – as you can provide them with a reference to a custom
//! configuration without messing around with global state.
//!
//! \return Since the init procedure needs access to the settings in order to
//! pass references to these to components the InitSettings methods returns a
//! pointer to the Settings object. It is guaranteed to be valid during the
//! lifetime of the application. If the settings could not be read from the
//! ArgsManager or settings have already been set this function returns a
//! nullptr.
const Settings *InitSettings(::ArgsManager &args);
}  // namespace esperanza

#endif  // UNIT_E_ESPERANZA_SETTINGS_INIT_H
