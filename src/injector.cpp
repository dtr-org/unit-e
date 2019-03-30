// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <injector.h>

namespace {
std::unique_ptr<UnitEInjector> injector = nullptr;
}

void UnitEInjector::Init(UnitEInjectorConfiguration config) {
  assert(!injector);
  injector = MakeUnique<UnitEInjector>(config);
  injector->Initialize();
}

void UnitEInjector::Destroy() {
  assert(injector);
  injector.reset();
}

UnitEInjector &GetInjector() {
  assert(injector);
  return *injector;
}
