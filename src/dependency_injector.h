// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_DEPENDENCY_INJECTOR_H
#define UNIT_E_DEPENDENCY_INJECTOR_H

#include <dependency.h>

#include <tinyformat.h>
#include <boost/optional.hpp>

#include <algorithm>
#include <atomic>
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

//! \brief Kahn's Algorithm for topological sorting
template <typename T, typename C = std::less<T>>
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
  // if the outgoing map still has edges in it then a circle was
  // detected which could never be added to the set of nodes with
  // no incoming edges (the circular back-reference would always
  // be an incoming one).
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

//! \brief reflects the types given as template parameters
//!
//! Returns a vector of type_index objects that describe the types given
//! in the template arguments. Useful in macros or other templates.
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

#define COMPONENT(NAME, TYPE, FACTORY, ...)                                     \
 private:                                                                       \
  Dependency<TYPE> m_component_##NAME = [this] {                                \
    return Registrator<TYPE>::Register<__VA_ARGS__>(                            \
        this, #NAME, [](InjectorType *injector) -> void {                       \
          injector->m_component_##NAME =                                        \
              Initializer<TYPE>::Managed<__VA_ARGS__>::Init(injector, FACTORY); \
        });                                                                     \
  }();                                                                          \
  Dependency<TYPE> Get(TYPE *) const { return m_component_##NAME; }

#define UNMANAGED_COMPONENT(NAME, TYPE, POINTER)                     \
 private:                                                            \
  Dependency<TYPE> m_component_##NAME = [this] {                     \
    return Registrator<TYPE>::Register<>(                            \
        this, #NAME, [](InjectorType *injector) -> void {            \
          injector->m_component_##NAME =                             \
              Initializer<TYPE>::Unmanaged::Init(injector, POINTER); \
        });                                                          \
  }();                                                               \
  Dependency<TYPE> Get(TYPE *) const { return m_component_##NAME; }

class InjectionError : public std::exception {
 public:
  ~InjectionError() override = default;
};

class UnregisteredDependenciesError : public InjectionError {
 private:
  mutable std::string m_error_message;

 public:
  std::vector<std::pair<std::string, std::type_index>> m_missingDependencies;
  explicit UnregisteredDependenciesError(
      std::vector<std::pair<std::string, std::type_index>>
          &&missingDependencies)
      : m_missingDependencies(std::move(missingDependencies)){};
  const char *what() const noexcept override {
    if (m_error_message.empty()) {
      std::ostringstream s;
      for (const auto &missingDependency : m_missingDependencies) {
        tfm::format(s, "%s requires %s, but that is not a known component\n",
                    missingDependency.first, missingDependency.second.name());
      }
      m_error_message = s.str();
    }
    return m_error_message.c_str();
  }
};

class CircularDependenciesError : public InjectionError {
  const char *what() const noexcept override {
    return "circular dependencies detected";
  }
};

class AlreadyInitializedError : public InjectionError {
  const char *what() const noexcept override {
    return "injector is already initialized (an attempt was made to re-initialize it)";
  }
};

struct ComponentError {
  std::type_index component;
  std::string what;
  ComponentError(const std::type_index &component, const char *what)
      : component(component), what(what) {}
};

class DependencyInitializationError : public InjectionError {
 private:
  std::type_index m_component;
  std::string m_error_message;

 public:
  DependencyInitializationError(const std::type_index &component, const char *what)
      : m_component(component), m_error_message(what) {}

  const char *what() const noexcept override {
    return m_error_message.c_str();
  }
};

class StoppingComponentsError : public InjectionError {
 private:
  std::vector<ComponentError> m_errors;
  mutable std::string m_error_message;

 public:
  explicit StoppingComponentsError(std::vector<ComponentError> &&errors)
      : m_errors(std::move(errors)) {}
  const char *what() const noexcept override {
    if (m_error_message.empty()) {
      m_error_message = tfm::format("Failed to stop %d components.", m_errors.size());
    }
    return m_error_message.c_str();
  }
  const std::vector<ComponentError> &GetErrors() { return m_errors; }
};

template <typename I>
class Injector {

 private:
  std::atomic_flag m_initialized = ATOMIC_FLAG_INIT;
  std::atomic_flag m_stopped = ATOMIC_FLAG_INIT;

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
    //! \brief function pointer that knows how to stop the component.
    Method m_stopper;
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
  struct Stopper {

   private:
    template <typename U>
    static void StopInternal(typename std::enable_if<std::is_same<void,
                                                                  decltype(static_cast<U *>(nullptr)->Stop())>::value,
                                                     I>::type *injector) {
      std::type_index type_index(typeid(U));
      U *i = static_cast<U *>(injector->m_components[type_index].m_instance);
      i->Stop();
    }

    template <typename U>
    static void StopInternal(void *) {}

   public:
    static void Stop(I *injector) {
      StopInternal<T>(injector);
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
      component.m_stopper = &Stopper<T>::Stop;
      component.m_deleter = &Deleter<T>::Delete;
      injector->m_components[typeIndex] = std::move(component);
      return nullptr;
    }
  };

  template <typename ComponentType>
  struct Initializer {
    static Component &GetComponent(I *injector) {
      return injector->m_components[typeid(ComponentType)];
    }
    template <typename... Args>
    struct Managed {
      template <typename F>
      static ComponentType *Init(I *injector, F f) {
        auto &component = GetComponent(injector);
        const auto dependencies = GatherDependencies(injector, component);
        std::unique_ptr<ComponentType> *returnTypeDeductionHint = nullptr;
        auto ptr = InjectorUtil::Invoker<Args...>::
                       Invoke(returnTypeDeductionHint, f, dependencies, 0)
                           .release();
        component.m_instance = ptr;
        return ptr;
      }
    };
    struct Unmanaged {
      template <typename T>
      static ComponentType *Init(I *injector, T *ptr) {
        auto &component = GetComponent(injector);
        component.m_instance = ptr;
        component.m_deleter = nullptr;
        return ptr;
      }
    };
  };

  Injector() {
    m_stopped.test_and_set();
  }

 private:
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
  //! \return If the dependency graph checks out: a vector of type_index objects
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
    if (m_initialized.test_and_set()) {
      throw AlreadyInitializedError();
    }
    std::vector<std::type_index> initializationOrder =
        DetermineInitializationOrder();
    for (const std::type_index &componentType : initializationOrder) {
      Component &component = m_components[componentType];
      try {
        component.m_initializer(static_cast<I *>(this));
      } catch (std::exception &err) {
        throw DependencyInitializationError(componentType, err.what());
      }
    }
    std::reverse(initializationOrder.begin(), initializationOrder.end());
    m_destructionOrder = std::move(initializationOrder);
    m_stopped.clear();
  }

  void Stop() {
    if (m_stopped.test_and_set()) {
      // already stopped
      return;
    }
    std::vector<ComponentError> errors;
    for (const std::type_index &component_type : m_destructionOrder) {
      if (m_components[component_type].m_stopper) {
        try {
          m_components[component_type].m_stopper(static_cast<I *>(this));
        } catch (std::exception &err) {
          errors.emplace_back(component_type, err.what());
        }
      }
    }
    if (!errors.empty()) {
      throw StoppingComponentsError(std::move(errors));
    }
  }

 protected:
  virtual ~Injector() {
    if (!m_initialized.test_and_set()) {
      // nothing to be done, was never initialized.
      return;
    }
    try {
      Stop();
    } catch (StoppingComponentsError &) {
      // see https://stackoverflow.com/a/130123/471478
    }
    for (const std::type_index &componentType : m_destructionOrder) {
      if (m_components[componentType].m_deleter) {
        m_components[componentType].m_deleter(static_cast<I *>(this));
      }
    }
  };
};

#endif  // UNIT_E_DEPENDENCY_INJECTOR_H
