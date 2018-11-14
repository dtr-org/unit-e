#include <test/esperanza/finalizationstate_utils.h>

uint160 RandValidatorAddr() {
  CKey key;
  key.MakeNewKey(true);
  return key.GetPubKey().GetID();
}
