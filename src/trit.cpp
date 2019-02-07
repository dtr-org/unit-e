#include <trit.h>

const Trit Trit::TRUE = Trit(true);
const Trit Trit::FALSE = Trit(false);
const Trit Trit::UNKNOWN = Trit();

Trit::Trit() noexcept : value(VALUE_UNKNOWN) {}

Trit::Trit(const bool truth) noexcept : value(truth ? VALUE_TRUE : VALUE_FALSE) {}
