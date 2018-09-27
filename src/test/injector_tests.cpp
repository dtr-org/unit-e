// Copyright (c) 2018 The unit-e core developers
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

class TestInjector : public Injector<TestInjector> {
  COMPONENT(A, TestNS::A, TestNS::A::Make)
  COMPONENT(X, TestNS::X, TestNS::X::Make)
  COMPONENT(C, TestNS::C, TestNS::C::Make, TestNS::A, TestNS::X)
};

class CircularInjector : public Injector<CircularInjector> {
  static std::unique_ptr<TestNS::A> MakeA(Dependency<C> c) { return nullptr; }
  static std::unique_ptr<TestNS::C> MakeC(Dependency<A> a) { return nullptr; }
  COMPONENT(A, TestNS::A, MakeA, TestNS::C)
  COMPONENT(C, TestNS::C, MakeA, TestNS::A)
};

class IncompleteInjector : public Injector<IncompleteInjector> {
  static std::unique_ptr<TestNS::A> MakeA(Dependency<C> c) { return nullptr; }
  COMPONENT(A, TestNS::A, MakeA, TestNS::C)
};

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
struct W {
  Dependency<A> a;
  W(Dependency<A> a) : a(a) { this->a->log->push_back("B"); }
  ~W() { this->a->log->push_back("~B"); }
  static std::unique_ptr<W> Make(Dependency<A> a) { return MakeUnique<W>(a); }
};
struct M {
  Dependency<A> a;
  Dependency<W> b;
  M(Dependency<A> a, Dependency<W> b) : a(a), b(b) {
    this->a->log->push_back("C");
  }
  ~M() { this->a->log->push_back("~C"); }
  static std::unique_ptr<M> Make(Dependency<A> a, Dependency<W> b) {
    return MakeUnique<M>(a, b);
  }
};
struct D {
  Dependency<A> a;
  Dependency<M> c;
  D(Dependency<A> a, Dependency<M> c) : a(a), c(c) {
    this->a->log->push_back("D");
  }
  ~D() { this->a->log->push_back("~D"); }
  static std::unique_ptr<D> Make(Dependency<A> a, Dependency<M> c) {
    return MakeUnique<D>(a, c);
  }
};

}  // namespace InjTestNS

class Inj : public Injector<Inj> {
  COMPONENT(B, InjTestNS::W, InjTestNS::W::Make, InjTestNS::A)
  COMPONENT(D, InjTestNS::D, InjTestNS::D::Make, InjTestNS::A, InjTestNS::M)
  COMPONENT(A, InjTestNS::A, InjTestNS::A::Make)
  COMPONENT(C, InjTestNS::M, InjTestNS::M::Make, InjTestNS::A, InjTestNS::W)
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
  std::vector<std::pair<int, int>> edges = {{5, 2}, {2, 3}, {3, 1},
                                            {4, 1}, {4, 0}, {5, 0}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {4, 5, 0, 2, 3, 1};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_2) {
  std::vector<std::pair<int, int>> edges = {{5, 2}, {2, 3}, {3, 1},
                                            {4, 1}, {0, 4}, {0, 5}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {0, 4, 5, 2, 3, 1};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_disconnected_graph) {
  std::vector<std::pair<int, int>> edges = {{0, 1}, {0, 2}, {3, 4},
                                            {3, 5}, {1, 2}, {4, 5}};
  const auto sorted = InjectorUtil::TopologicalSort<int>(edges);
  BOOST_CHECK(sorted);
  const std::vector<int> result = sorted.get();
  const std::vector<int> expected = {0, 1, 2, 3, 4, 5};
  BOOST_CHECK_EQUAL(vec2str(result), vec2str(expected));
}

BOOST_AUTO_TEST_CASE(topological_sort_complex_strings) {
  std::string str[6] = {"0", "1", "2", "3", "4", "5"};
  std::vector<std::pair<std::string *, std::string *>> edges = {
      {&str[5], &str[2]}, {&str[2], &str[3]}, {&str[3], &str[1]},
      {&str[4], &str[1]}, {&str[0], &str[4]}, {&str[0], &str[5]}};
  const boost::optional<std::vector<std::string *>> sorted =
      InjectorUtil::TopologicalSort<std::string *,
                                    InjectorUtil::LessPtr<std::string>>(edges);
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
  Dependency<TestNS::C> c = tj.GetC();
  BOOST_CHECK_EQUAL(c->Foo(), "A+X");
}

BOOST_AUTO_TEST_CASE(initialize_all_components) {
  Inj inj;
  BOOST_CHECK(inj.GetA() == nullptr);
  BOOST_CHECK(inj.GetB() == nullptr);
  BOOST_CHECK(inj.GetC() == nullptr);
  BOOST_CHECK(inj.GetD() == nullptr);
  inj.Initialize();
  BOOST_CHECK(inj.GetA() != nullptr);
  BOOST_CHECK(inj.GetB() != nullptr);
  BOOST_CHECK(inj.GetC() != nullptr);
  BOOST_CHECK(inj.GetD() != nullptr);
}

BOOST_AUTO_TEST_CASE(initialize_all_dependencies) {
  Inj inj;
  inj.Initialize();
  auto a = inj.GetA();
  auto b = inj.GetB();
  auto c = inj.GetC();
  auto d = inj.GetD();
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

BOOST_AUTO_TEST_CASE(initialization_and_destruction_order) {
  std::shared_ptr<std::vector<std::string>> log;
  {
    Inj inj;
    inj.Initialize();
    log = inj.GetA()->log;
  }
  BOOST_CHECK_EQUAL(vec2str(*log), "A;B;C;D;~D;~C;~B;~A;");
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

BOOST_AUTO_TEST_SUITE_END()
