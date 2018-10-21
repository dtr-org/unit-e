// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_DEPENDENCY_INJECTOR_H
#define UNIT_E_DEPENDENCY_INJECTOR_H

#include <dependency.h>

#include <tinyformat.h>
#include <boost/optional.hpp>

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace InjectorUtil {

template <typename T>
struct Less {
  bool operator()(const T a, const T b) const { return a < b; }
};

template <typename T>
struct LessPtr {
  bool operator()(const T *a, const T *b) const { return *a < *b; }
};

//! \brief Kahn's Algorithm for topological sorting
template <typename T, typename C = Less<T>>
static boost::optional<std::vector<T>> TopologicalSort(
    const std::vector<std::pair<T, T>> &edges) {
  std::vector<T> result;
  std::map<T, std::set<T, C>, C> incoming;
  std::map<T, std::set<T, C>, C> outgoing;
  for (const auto &edge : edges) {
    incoming[edge.second].insert(edge.first);
    incoming[edge.first];
    outgoing[edge.first].insert(edge.second);
  }
  std::set<T, C> noIncoming;
  for (const auto &inMap : incoming) {
    if (inMap.second.empty()) {
      noIncoming.insert(inMap.first);
    }
  }
  while (!noIncoming.empty()) {
    const T node = *noIncoming.cbegin();
    result.push_back(node);
    noIncoming.erase(noIncoming.cbegin());
    for (const auto &to : outgoing[node]) {
      incoming[to].erase(node);
      if (incoming[to].empty()) {
        noIncoming.insert(to);
      }
    }
    outgoing.erase(node);
  }
  if (outgoing.empty()) {
    return boost::optional<std::vector<T>>(std::move(result));
  }
  return boost::none;
}

template <typename... TS>
struct TypeInfoHelper;

template <typename T, typename... TS>
struct TypeInfoHelper<T, TS...> {
  static void Get(std::vector<std::type_index> &acc) {
    std::type_index typeIndex(typeid(T));
    acc.push_back(typeIndex);
    TypeInfoHelper<TS...>::Get(acc);
  }
};

template <>
struct TypeInfoHelper<> {
  static void Get(std::vector<std::type_index> &acc) {}
};

template <typename... TS>
std::vector<std::type_index> TypeInfo() {
  std::vector<std::type_index> typeIndices;
  TypeInfoHelper<TS...>::Get(typeIndices);
  return typeIndices;
}

template <typename... Args>
struct Invoker;

template <typename Arg, typename... Remaining>
struct Invoker<Arg, Remaining...> {
  template <typename R, typename F, typename... Args>
  static inline R Invoke(R *r, F f, const std::vector<void *> &args, size_t i,
                         Args... completed) {
    return Invoker<Remaining...>::Invoke(r, f, args, i + 1, completed...,
                                         static_cast<Arg *>(args[i]));
  }
};

template <>
struct Invoker<> {
  template <typename R, typename F, typename... Args>
  static inline R Invoke(R *r, F f, const std::vector<void *> &, size_t,
                         Args... args) {
    return f(args...);
  }
};

}  // namespace InjectorUtil

#define COMPONENT(NAME, TYPE, FACTORY, ...)                                 \
 private:                                                                   \
  static void Init##NAME(InjectorType *injector) {                          \
    std::type_index typeIndex(typeid(TYPE));                                \
    Component &component = injector->m_components[typeIndex];               \
    const auto dependencies = GatherDependencies(injector, component);      \
    std::unique_ptr<TYPE> *returnTypeDeductionHint = nullptr;               \
    std::unique_ptr<TYPE> ptr = InjectorUtil::Invoker<__VA_ARGS__>::Invoke( \
        returnTypeDeductionHint, FACTORY, dependencies, 0);                 \
    injector->m_component_##NAME = ptr.release();                           \
    component.m_instance = injector->m_component_##NAME;                    \
  }                                                                         \
  static Dependency<TYPE> Register##NAME(InjectorType *injector) {          \
    return Registrator<TYPE>::Register<__VA_ARGS__>(injector, #NAME,        \
                                                    &Init##NAME);           \
  }                                                                         \
  Dependency<TYPE> m_component_##NAME = Register##NAME(this);               \
                                                                            \
 public:                                                                    \
  Dependency<TYPE> Get##NAME() { return m_component_##NAME; }

class InjectionError {
 public:
  virtual std::string ErrorMessage() const = 0;
  virtual ~InjectionError() = default;
};

class UnregisteredDependenciesError : public InjectionError {
 public:
  std::vector<std::pair<std::string, std::type_index>> m_missingDependencies;
  explicit UnregisteredDependenciesError(
      std::vector<std::pair<std::string, std::type_index>>
          &&missingDependencies)
      : m_missingDependencies(std::move(missingDependencies)){};
  std::string ErrorMessage() const override {
    std::ostringstream s;
    for (const auto &missingDependency : m_missingDependencies) {
      tfm::format(s, "%s requires %s, but that is not a known component\n",
                  missingDependency.first, missingDependency.second.name());
    }
    return s.str();
  }
};

class CircularDependenciesError : public InjectionError {
  std::string ErrorMessage() const override {
    return "circular dependencies detected";
  }
};

template <typename I>
class Injector {

 protected:
  // `I` is not available in derived classes, a using declaration makes it
  // available though. Although the derived class will fill its own name in `I`,
  // that name is not known from the COMPONENT macro.
  using InjectorType = I;

  // a function pointer to a function that takes an Injector as its first
  // argument. Used for static methos to act as if they were non-static member
  // methods, but we need a stable function pointer to them (thus static) and
  // they need access to the injector (hence it's passed in).
  using Method = void (*)(InjectorType *);

  struct Component {
    std::string m_name;
    std::vector<std::type_index> m_dependencies;
    //! \brief function pointer that knows how to create the component.
    Method m_initializer;
    //! \brief function pointer that knows how to delete the component.
    Method m_deleter;
    void *m_instance = nullptr;
  };

  std::map<std::type_index, Component> m_components;
  std::vector<std::type_index> m_destructionOrder;

  static std::vector<void *> GatherDependencies(I *injector,
                                                const Component &component) {
    std::vector<void *> dependentComponents;
    for (const auto &dep : component.m_dependencies) {
      Component &dependency = injector->m_components[dep];
      dependentComponents.push_back(dependency.m_instance);
    }
    return dependentComponents;
  }

  template <typename T>
  struct Deleter {
    static void Delete(I *injector) {
      std::type_index typeIndex(typeid(T));
      T *i = static_cast<T *>(injector->m_components[typeIndex].m_instance);
      delete i;
    }
  };

  template <typename T>
  struct Registrator {
    template <typename... Deps>
    static Dependency<T> Register(I *injector, const std::string &name,
                                  Method init) {
      std::type_index typeIndex(typeid(T));
      Component component;
      component.m_name = name;
      component.m_dependencies = InjectorUtil::TypeInfo<Deps...>();
      component.m_initializer = init;
      component.m_deleter = &Deleter<T>::Delete;
      injector->m_components[typeIndex] = std::move(component);
      return nullptr;
    }
  };

  virtual ~Injector() {
    for (const std::type_index &componentType : m_destructionOrder) {
      m_components[componentType].m_deleter(static_cast<I *>(this));
    }
  };

 private:
  void InitializeDependency(const std::type_index &componentType) {
    Component &component = m_components[componentType];
    try {
      component.m_initializer(static_cast<I *>(this));
    } catch (...) {
      throw std::runtime_error(
          tfm::format("error initializing dependency %s of type %s",
                      component.m_name, componentType.name()));
    }
  }

  void CheckDependencies() const {
    std::vector<std::pair<std::string, std::type_index>> missingComponents;
    for (const auto &component : m_components) {
      for (const auto &dependsOn : component.second.m_dependencies) {
        if (m_components.count(dependsOn) == 0) {
          missingComponents.emplace_back(component.second.m_name, dependsOn);
        }
      }
    }
    if (!missingComponents.empty()) {
      throw UnregisteredDependenciesError(std::move(missingComponents));
    }
  }

 public:
  //! \brief check and compute initialization order
  //!
  //! This function is useful to check the correctness of the Injector, sort of
  //! like a dry run. It will throw the same exceptions as Initialize() would,
  //! but will not actually initialize any component.
  //!
  //! \throws UnregisteredDependenciesError if the Injector contains a component
  //! that depends on a type which is not bound (i.e. no Component of that type
  //! is defined)
  //!
  //! \throws CircularDependenciesError if the dependency graph contains cycles.
  //!
  //! \return If the dependency graph checks out: a vector of
  //! type_index objects
  std::vector<std::type_index> DetermineInitializationOrder() const {
    CheckDependencies();
    std::vector<std::pair<std::type_index, std::type_index>> dependencyGraph;
    for (const auto &component : m_components) {
      for (const auto &dependsOn : component.second.m_dependencies) {
        dependencyGraph.emplace_back(dependsOn, component.first);
      }
    }
    const auto initOrder = InjectorUtil::TopologicalSort(dependencyGraph);
    if (initOrder) {
      return initOrder.get();
    }
    throw CircularDependenciesError();
  }

  void Initialize() {
    std::vector<std::type_index> initializationOrder =
        DetermineInitializationOrder();
    for (const std::type_index &componentType : initializationOrder) {
      InitializeDependency(componentType);
    }
    std::reverse(initializationOrder.begin(), initializationOrder.end());
    m_destructionOrder = std::move(initializationOrder);
  }
};

#endif  // UNIT_E_DEPENDENCY_INJECTOR_H
