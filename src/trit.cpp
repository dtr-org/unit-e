#include <trit.h>

const Trit Trit::T_TRUE = Trit(true);
const Trit Trit::T_FALSE = Trit(false);
const Trit Trit::T_UNKNOWN = Trit();

Trit::Trit() noexcept : value(VALUE_UNKNOWN) {}

Trit::Trit(const bool truth) noexcept : value(truth ? VALUE_TRUE : VALUE_FALSE) {}
