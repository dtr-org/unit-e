// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dependency_injector.h>

#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

template <typename T>
struct ToString {
  std::string operator()(const T &thing) { return std::to_string(thing); }
};

template <>
struct ToString<std::string> {
  std::string operator()(const std::string &thing) { return thing; }
};

template <>
struct ToString<std::string *> {
  std::string operator()(const std::string *thing) { return *thing; }
};

template <>
struct ToString<std::type_index> {
  std::string operator()(const std::type_index &thing) { return thing.name(); }
};

template <typename T>
std::string vec2str(std::vector<T> vec) {
  ToString<T> toString;
  std::string str;
  for (const auto &elem : vec) {
    str += toString(elem);
    str += ";";
  }
  return str;
}

namespace TestNS {
struct A {
  std::string Foo() { return "A"; }
  static std::unique_ptr<A> Make() { return MakeUnique<A>(); }
};
struct X {
  std::string Foo() { return "X"; }
  static std::unique_ptr<X> Make() { return MakeUnique<X>(); }
};
struct C {
  Dependency<A> m_a;
  Dependency<X> m_x;
  std::string Foo() { return m_a->Foo() + "+" + m_x->Foo(); }
  C(Dependency<A> a, Dependency<X> x) : m_a(a), m_x(x) {}

  static std::unique_ptr<C> Make(Dependency<A> a, Dependency<X> x) {
    return MakeUnique<C>(a, x);
  }
};

std::string Z(const int *a, const double *b) {
  return std::to_string(*a) + " " + std::to_string(*b);
}
}  // namespace TestNS

namespace {

class TestInjector : public Injector<TestInjector> {
  COMPONENT(A, TestNS::A, TestNS::A::Make)
  COMPONENT(X, TestNS::X, TestNS::X::Make)
  COMPONENT(C, TestNS::C, TestNS::C::Make, TestNS::A, TestNS::X)
 public:
  template <typename T>
  Dependency<T> Get() const {
    // the passed nullptr merely serves to select the right getter
    return Get(static_cast<T *>(nullptr));
  }
};

class CircularInjector : public Injector<CircularInjector> {
  static std::unique_ptr<TestNS::A> MakeA(Dependency<TestNS::C> c) {
    return nullptr;
  }
  static std::unique_ptr<TestNS::C> MakeC(Dependency<TestNS::A> a) {
    return nullptr;
  }
  COMPONENT(A, TestNS::A, MakeA, TestNS::C)
  COMPONENT(C, TestNS::C, MakeC, TestNS::A)
 public:
  template <typename T>
  Dependency<T> Get() const {
    // the passed nullptr merely serves to select the right getter
    return Get(static_cast<T *>(nullptr));
  }
};

class IncompleteInjector : public Injector<IncompleteInjector> {
  static std::unique_ptr<TestNS::A> MakeA(Dependency<TestNS::C> c) {
    return nullptr;
  }
  COMPONENT(A, TestNS::A, MakeA, TestNS::C)
 public:
  template <typename T>
  Dependency<T> Get() const {
    // the passed nullptr merely serves to select the right getter
    return Get(static_cast<T *>(nullptr));
  }
};

struct ComplexThing {
  std::uint64_t a;
  std::uint64_t b;
  ComplexThing(std::uint64_t a, std::uint64_t b) : a(a), b(b) {}
};

std::unique_ptr<ComplexThing> global_ptr = MakeUnique<ComplexThing>(17, 13);

}  // namespace

namespace InjTestNS {

struct A {
  std::shared_ptr<std::vector<std::string>> log;
  A() {}
  ~A() { log->push_back("~A"); }
  static std::unique_ptr<A> Make() {
    std::unique_ptr<A> ptr = MakeUnique<A>();
    ptr->log = std::make_shared<std::vector<std::string>>();
    ptr->log->push_back("A");
    return ptr;
  }
};
struct B {
  Dependency<A> a;
  B(Dependency<A> a) : a(a) { this->a->log->push_back("B"); }
  ~B() { this->a->log->push_back("~B"); }
  static std::unique_ptr<B> Make(Dependency<A> a) { return MakeUnique<B>(a); }
};
struct C {
  Dependency<A> a;
  Dependency<B> b;
  C(Dependency<A> a, Dependency<B> b) : a(a), b(b) {
    this->a->log->push_back("C");
  }
  ~C() { this->a->log->push_back("~C"); }
  void Stop() { this->a->log->push_back("C::Stop()"); }
  static std::unique_ptr<C> Make(Dependency<A> a, Dependency<B> b) {
    return MakeUnique<C>(a, b);
  }
};
struct D {
  Dependency<A> a;
  Dependency<C> c;
  D(Dependency<A> a, Dependency<C> c) : a(a), c(c) {
    this->a->log->push_back("D");
  }
  ~D() { this->a->log->push_back("~D"); }
  static std::unique_ptr<D> Make(Dependency<A> a, Dependency<C> c) {
    return MakeUnique<D>(a, c);
  }
};
struct Q {
  Dependency<ComplexThing> complex_thing;
  Q(Dependency<ComplexThing> complex_thing) : complex_thing(complex_thing) {}
  static std::unique_ptr<Q> Make(Dependency<ComplexThing> complex_thing) {
    return MakeUnique<Q>(complex_thing);
  }
};

}  // namespace InjTestNS

namespace {

class Inj : public Injector<Inj> {
  COMPONENT(B, InjTestNS::B, InjTestNS::B::Make, InjTestNS::A)
  COMPONENT(D, InjTestNS::D, InjTestNS::D::Make, InjTestNS::A, InjTestNS::C)
  COMPONENT(A, InjTestNS::A, InjTestNS::A::Make)
  COMPONENT(C, InjTestNS::C, InjTestNS::C::Make, InjTestNS::A, InjTestNS::B)
 public:
  template <typename T>
  Dependency<T> Get() const {
    // the passed nullptr merely serves to select the right getter
    return Get(static_cast<T *>(nullptr));
  }
};

class UnmanagedInj : public Injector<UnmanagedInj> {
  static ComplexThing *GetGlobalPtr(UnmanagedInj *) { return global_ptr.get(); }
  UNMANAGED_COMPONENT(One, ComplexThing, GetGlobalPtr)
  COMPONENT(Two, InjTestNS::Q, InjTestNS::Q::Make, ComplexThing)
 public:
  template <typename T>
  Dependency<T> Get() const {
    // the passed nullptr merely serves to select the right getter
    return Get(static_cast<T *>(nullptr));
  }
};

}  // namespace

template <typename T>
struct LessPtr {
  bool operator()(const T *a, const T *b) const { return *a < *b; }
};

BOOST_AUTO_TEST_SUITE(dependency_injector_tests)

BOOST_AUTO_TEST_CASE(topological_sort_empty_graph) {
  std::vector<std::pair<int, int>> edges;
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected;
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_one_edge_graph) {
  std::vector<std::pair<int, int>> edges = {{1, 2}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {1, 2};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_cycle) {
  std::vector<std::pair<int, int>> edges = {{1, 2}, {2, 1}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(!sorted);
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_1) {
  std::vector<std::pair<int, int>> edges = {{5, 2}, {2, 3}, {3, 1}, {4, 1}, {4, 0}, {5, 0}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {4, 5, 0, 2, 3, 1};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_2) {
  std::vector<std::pair<int, int>> edges = {{5, 2}, {2, 3}, {3, 1}, {4, 1}, {0, 4}, {0, 5}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {0, 4, 5, 2, 3, 1};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_disconnected_graph) {
  std::vector<std::pair<int, int>> edges = {{0, 1}, {0, 2}, {3, 4}, {3, 5}, {1, 2}, {4, 5}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {0, 1, 2, 3, 4, 5};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_strings) {
  std::string str[6] = {"0", "1", "2", "3", "4", "5"};
  std::vector<std::pair<std::string *, std::string *>> edges = {
      {&str[5], &str[2]}, {&str[2], &str[3]}, {&str[3], &str[1]}, {&str[4], &str[1]}, {&str[0], &str[4]}, {&str[0], &str[5]}};
  const boost::optional<std::vector<std::string *>> sorted =
      InjectorUtil::TopologicalSort<std::string *, LessPtr<std::string>>(edges);
  BOOST_CHECK(sorted);
  const std::vector<std::string *> result = sorted.get();
  const std::vector<std::string> expected = {"0", "4", "5", "2", "3", "1"};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(type_info_helper) {
  std::vector<std::type_index> ixs =
      InjectorUtil::TypeInfo<int, std::string, char>();
  BOOST_CHECK_EQUAL(typeid(int).name(), ixs[0].name());
  BOOST_CHECK_EQUAL(typeid(std::string).name(), ixs[1].name());
  BOOST_CHECK_EQUAL(typeid(char).name(), ixs[2].name());
}

BOOST_AUTO_TEST_CASE(invoker) {
  int seven = 7;
  double pi = 3.14;
  std::vector<void *> v;
  v.push_back(&seven);
  v.push_back(&pi);
  std::string *returnTypeDeductionHint = nullptr;
  std::string result = InjectorUtil::Invoker<int, double>::Invoke(
      returnTypeDeductionHint, TestNS::Z, v, 0);
  std::string expected = std::to_string(seven) + " " + std::to_string(pi);
  BOOST_CHECK_EQUAL(result, expected);
}

BOOST_AUTO_TEST_CASE(injector) {
  TestInjector tj;
  tj.Initialize();
  Dependency<TestNS::C> c = tj.Get<TestNS::C>();
  BOOST_CHECK_EQUAL(c->Foo(), "A+X");
}

BOOST_AUTO_TEST_CASE(initialize_all_components) {
  Inj inj;
  BOOST_CHECK(inj.Get<InjTestNS::A>() == nullptr);
  BOOST_CHECK(inj.Get<InjTestNS::B>() == nullptr);
  BOOST_CHECK(inj.Get<InjTestNS::C>() == nullptr);
  BOOST_CHECK(inj.Get<InjTestNS::D>() == nullptr);
  inj.Initialize();
  BOOST_CHECK(inj.Get<InjTestNS::A>() != nullptr);
  BOOST_CHECK(inj.Get<InjTestNS::B>() != nullptr);
  BOOST_CHECK(inj.Get<InjTestNS::C>() != nullptr);
  BOOST_CHECK(inj.Get<InjTestNS::D>() != nullptr);
}

BOOST_AUTO_TEST_CASE(initialize_all_dependencies) {
  Inj inj;
  inj.Initialize();
  auto a = inj.Get<InjTestNS::A>();
  auto b = inj.Get<InjTestNS::B>();
  auto c = inj.Get<InjTestNS::C>();
  auto d = inj.Get<InjTestNS::D>();
  BOOST_CHECK(a != nullptr);
  BOOST_CHECK(b != nullptr);
  BOOST_CHECK(c != nullptr);
  BOOST_CHECK(d != nullptr);
  BOOST_CHECK_EQUAL(b->a, a);
  BOOST_CHECK_EQUAL(c->a, a);
  BOOST_CHECK_EQUAL(c->b, b);
  BOOST_CHECK_EQUAL(d->a, a);
  BOOST_CHECK_EQUAL(d->c, c);
}

BOOST_AUTO_TEST_CASE(do_not_tear_down_unmanaged_dependencies) {
  {
    UnmanagedInj inj;
    inj.Initialize();
    auto one = inj.Get<ComplexThing>();
    auto two = inj.Get<InjTestNS::Q>();
    BOOST_CHECK(one != nullptr);
    BOOST_CHECK(two != nullptr);
    BOOST_CHECK_EQUAL(two->complex_thing, one);
  }
  // injector will be destroyed here, should not try to free global_ptr object
  BOOST_CHECK_EQUAL(global_ptr->a, 17);
  BOOST_CHECK_EQUAL(global_ptr->b, 13);
}

BOOST_AUTO_TEST_CASE(initialization_and_destruction_order) {
  std::shared_ptr<std::vector<std::string>> log;
  {
    Inj inj;
    inj.Initialize();
    log = inj.Get<InjTestNS::A>()->log;
  }
  BOOST_CHECK_EQUAL(vec2str(*log), "A;B;C;D;C::Stop();~D;~C;~B;~A;");
}

BOOST_AUTO_TEST_CASE(incomplete_dependencies) {
  IncompleteInjector inj;
  BOOST_CHECK_THROW(inj.DetermineInitializationOrder(),
                    UnregisteredDependenciesError);
}

BOOST_AUTO_TEST_CASE(circular_dependencies) {
  CircularInjector inj;
  BOOST_CHECK_THROW(inj.DetermineInitializationOrder(),
                    CircularDependenciesError);
}

BOOST_AUTO_TEST_CASE(initialize_twice_id) {
  Inj inj;
  BOOST_CHECK_NO_THROW(inj.Initialize());
  BOOST_CHECK_THROW(inj.Initialize(),
                    AlreadyInitializedError);
}

BOOST_AUTO_TEST_SUITE_END()
