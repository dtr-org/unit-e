// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_DEPENDENCY_H
#define UNIT_E_DEPENDENCY_H

template<typename T>
using Dependency = T*;

//! \brief a module which is provided by but has a lifecycle independent from the injector.
template <typename T>
struct Ptr {
  T *obj;

  Ptr(T *obj) : obj(obj) {}
};

#endif //UNIT_E_DEPENDENCY_H
