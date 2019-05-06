// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/MIT.

#include <trit.h>

const Trit Trit::True = Trit(true);
const Trit Trit::False = Trit(false);
const Trit Trit::Unknown = Trit();

Trit::Trit() noexcept : value(value_unknown) {}

Trit::Trit(const bool truth) noexcept : value(truth ? value_true : value_false) {}
