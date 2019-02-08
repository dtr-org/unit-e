#include <trit.h>

const Trit Trit::True = Trit(true);
const Trit Trit::False = Trit(false);
const Trit Trit::Unknown = Trit();

Trit::Trit() noexcept : value(value_unknown) {}

Trit::Trit(const bool truth) noexcept : value(truth ? value_true : value_false) {}
