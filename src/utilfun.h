// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_UTILFUN_H
#define UNITE_UTILFUN_H

#include <functional>

//! left-associative fold of anything that can be iterated over.
template <template <typename...> class Container, class A, class B,
          typename Combiner, typename... Args>
B fold_left(const Combiner combine, const B start,
            const Container<A, Args...> &container) {
  B current = start;
  for (auto it = container.cbegin(); it < container.cend(); ++it) {
    current = combine(current, *it);
  }
  return current;
}

//! right-associative fold of anything that can be iterated over.
template <template <typename...> class Container, class A, class B,
          typename Combiner, typename... Args>
B fold_right(const Combiner combine, const B start,
             const Container<A, Args...> &container) {
  B current = start;
  for (auto it = container.crbegin(); it < container.crend(); ++it) {
    current = combine(*it, current);
  }
  return current;
}

//! left-associative fold that tracks all successive reduced values.
template <template <typename...> class Container, class A, class B,
          typename Combiner, typename... Args>
Container<B, Args...> scan_left(const Combiner combine, const B start,
                                const Container<A, Args...> &container) {
  B current = start;
  Container<B, Args...> result;
  result.push_back(start);
  for (auto it = container.cbegin(); it < container.cend(); ++it) {
    current = combine(current, *it);
    result.push_back(current);
  }
  return result;
}

//! zip two containers using a generic zipper function.
//!
//! Example: zip_with (plus, [1, 2, 3], [4, 5, 6]) -> [5, 7, 9]
//!
//! The resulting container's size is min(left.size, right.size).
template <template <typename...> class Container, class A, class B,
          typename Zipper, typename... Args, typename... Args2>
Container<typename std::result_of<Zipper(A, B)>::type> zip_with(
    const Zipper zipper, const Container<A, Args...> &left,
    const Container<B, Args2...> &right) {
  Container<typename std::result_of<Zipper(A, B)>::type> result;
  auto itl = left.cbegin();
  auto itr = right.cbegin();
  for (; itl < left.cend() && itr < right.cend(); ++itl, ++itr) {
    result.push_back(zipper(*itl, *itr));
  }
  return result;
}

//! take the longest prefix in which each element satisfies the given predicate.
template <template <typename...> class Container, class A, typename Predicate,
          typename... Args>
Container<A, Args...> take_while(const Predicate predicate,
                                 const Container<A, Args...> &container) {
  Container<A, Args...> result;
  for (auto it = container.cbegin(); it < container.cend(); ++it) {
    if (predicate(*it)) {
      result.push_back(*it);
    } else {
      break;
    }
  }
  return result;
}

//! drop the longest prefix in which each element satisfies the given predicate.
template <template <typename...> class Container, class A, typename Predicate,
          typename... Args>
Container<A, Args...> drop_while(const Predicate predicate,
                                 const Container<A, Args...> &container) {
  Container<A, Args...> result;
  auto it = container.cbegin();
  for (; it < container.cend(); ++it) {
    if (!predicate(*it)) {
      break;
    }
  }
  for (; it < container.cend(); ++it) {
    result.push_back(*it);
  }
  return result;
}

//! return a new container that contains only the elements the predicate
//! applies to.
template <template <typename...> class Container, class A, typename Predicate,
          typename... Args>
Container<A, Args...> filter(const Predicate predicate,
                             const Container<A, Args...> &container) {
  Container<A, Args...> result;
  for (auto it = container.cbegin(); it < container.cend(); ++it) {
    if (predicate(*it)) {
      result.push_back(*it);
    }
  }
  return result;
}

//! return a new container that contains all the elements of the original
//! except for the ones which a predicate applies to.
template <template <typename...> class Container, class A, typename Predicate,
          typename... Args>
Container<A, Args...> filter_not(const Predicate predicate,
                                 const Container<A, Args...> &container) {
  Container<A, Args...> result;
  for (auto it = container.cbegin(); it < container.cend(); ++it) {
    if (!predicate(*it)) {
      result.push_back(*it);
    }
  }
  return result;
}

#endif  // UNITE_UTILFUN_H
