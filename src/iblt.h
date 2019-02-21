// Copyright (c) 2019 The Unit-e developers
// Copyright (c) 2014 Gavin Andresen
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_IBLT_H
#define UNITE_IBLT_H

#include <stdlib.h>
#include <cinttypes>
#include <set>
#include <vector>

#include <hash.h>
#include <iblt_params.h>
#include <serialize.h>

//
// Invertible Bloom Lookup Table implementation
// References:
//
// "What's the Difference? Efficient Set Reconciliation
// without Prior Context" by Eppstein, Goodrich, Uyeda and
// Varghese
//
// "Invertible Bloom Lookup Tables" by Goodrich and
// Mitzenmacher
//

template <typename T>
std::vector<uint8_t> ToVec(T number) {
  std::vector<uint8_t> v(sizeof(T));
  for (size_t i = 0; i < sizeof(T); i++) {
    v.at(i) = (number >> i * 8) & 0xff;
  }
  return v;
}

template <typename TKey, typename TCount, size_t ValueSize>
class IBLT {
 public:
  using TEntriesMap = std::map<TKey, std::vector<uint8_t>>;

  explicit IBLT(size_t expected_symmetric_difference) {
    const IbltParams optimal_params = IbltParams::FindOptimal(expected_symmetric_difference);
    m_num_hashes = optimal_params.num_hashes;

    const auto entries_num = ComputeEntriesNum(expected_symmetric_difference, optimal_params);

    m_hash_table.resize(entries_num);
  }

  IBLT() = default;

  IBLT(size_t num_entries, uint8_t num_hashes)
      : m_hash_table(num_entries), m_num_hashes(num_hashes) {
    assert(IsValid());
  }

  void Insert(TKey k, const std::vector<uint8_t> &v) {
    _insert(1, k, v);
  }

  void Erase(TKey k, const std::vector<uint8_t> &v) {
    _insert(-1, k, v);
  }

  // Returns true if a result is definitely found or not
  // found. If not found, result will be empty.
  // Returns false if overloaded and we don't know whether or
  // not k is in the table.
  bool Get(TKey k, std::vector<uint8_t> &result) const {
    result.clear();

    std::vector<uint8_t> kvec = ToVec(k);

    size_t bucketsPerHash = m_hash_table.size() / m_num_hashes;
    for (size_t i = 0; i < m_num_hashes; i++) {
      size_t startEntry = i * bucketsPerHash;

      uint32_t h = MurmurHash3(i, kvec);
      const IBLT::HashTableEntry &entry = m_hash_table.at(startEntry + (h % bucketsPerHash));

      if (entry.Empty()) {
        // Definitely not in table. Leave
        // result empty, return true.
        return true;
      } else if (entry.IsPure()) {
        if (entry.key_sum == k) {
          // Found!
          result.assign(entry.value_sum.begin(), entry.value_sum.end());
          return true;
        } else {
          // Definitely not in table.
          return true;
        }
      }
    }

    // Don't know if k is in table or not; "peel" the IBLT to try to find
    // it:
    IBLT<TKey, TCount, ValueSize> peeled = *this;
    size_t nErased = 0;
    for (size_t i = 0; i < peeled.m_hash_table.size(); i++) {
      auto &entry = peeled.m_hash_table.at(i);
      if (entry.IsPure()) {
        if (entry.key_sum == k) {
          // Found!
          result.assign(entry.value_sum.begin(), entry.value_sum.end());
          return true;
        }
        ++nErased;
        // _insert will reiterate table and might change our entry because it
        // is a reference
        const auto value_sum_copy = entry.value_sum;
        peeled._insert(-entry.count, entry.key_sum, value_sum_copy);
      }
    }
    if (nErased > 0) {
      // Recurse with smaller IBLT
      return peeled.Get(k, result);
    }
    return false;
  }

  // Adds entries to the given sets:
  //  positive is all entries that were inserted
  //  negative is all entreis that were erased but never added (or
  //   if the IBLT = A-B, all entries in B that are not in A)
  // Returns true if all entries could be decoded, false otherwise.
  bool ListEntries(TEntriesMap &positive,
                   TEntriesMap &negative) const {
    IBLT<TKey, TCount, ValueSize> peeled = *this;

    size_t nErased = 0;
    do {
      nErased = 0;
      for (size_t i = 0; i < peeled.m_hash_table.size(); i++) {
        IBLT::HashTableEntry &entry = peeled.m_hash_table.at(i);
        if (entry.IsPure()) {
          if (entry.count == 1) {
            positive.emplace(entry.key_sum, entry.value_sum);
          } else {
            negative.emplace(entry.key_sum, entry.value_sum);
          }
          // _insert will reiterate table and might change our entry because it
          // is a reference
          const auto copy = entry.value_sum;
          peeled._insert(-entry.count, entry.key_sum, copy);
          ++nErased;
        }
      }
    } while (nErased > 0);

    // If any buckets for one of the hash functions is not empty,
    // then we didn't peel them all:
    for (size_t i = 0; i < peeled.m_hash_table.size() / m_num_hashes; i++) {
      if (!peeled.m_hash_table.at(i).Empty()) return false;
    }
    return true;
  }

  // Subtract two IBLTs
  IBLT<TKey, TCount, ValueSize> operator-(const IBLT<TKey, TCount, ValueSize> &other) const {
    // IBLT's must be same params/size:
    assert(m_hash_table.size() == other.m_hash_table.size());

    IBLT<TKey, TCount, ValueSize> result(*this);
    for (size_t i = 0; i < m_hash_table.size(); i++) {
      IBLT::HashTableEntry &e1 = result.m_hash_table.at(i);
      const IBLT::HashTableEntry &e2 = other.m_hash_table.at(i);
      e1.count -= e2.count;
      e1.key_sum ^= e2.key_sum;
      e1.key_check ^= e2.key_check;
      if (e1.Empty()) {
        e1.value_sum.clear();
      } else {
        e1.AddValue(e2.value_sum);
      }
    }

    return result;
  }

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_hash_table);
    READWRITE(m_num_hashes);
  }

  size_t Size() const {
    assert(IsValid());

    size_t sum = 0;
    for (const auto &entry : m_hash_table) {
      sum += llabs(entry.count);
    }

    return sum / m_num_hashes;
  }

  IBLT<TKey, TCount, ValueSize> CloneEmpty() const {
    return IBLT<TKey, TCount, ValueSize>(m_hash_table.size(), m_num_hashes);
  }

  bool IsValid() const {
    // When we are creating new iblt - we can adjust those values to optimal,
    // but if we receive them from network - they must meet these criteria

    if (m_num_hashes == 0) {
      return false;
    }

    return m_hash_table.size() % m_num_hashes == 0;
  }

  static size_t ComputeEntriesNum(size_t expected_items_count, boost::optional<IbltParams> params = {}) {
    const IbltParams iblt_params = params ? params.get() : IbltParams::FindOptimal(expected_items_count);

    auto entries_num = static_cast<size_t>(iblt_params.overhead * expected_items_count);

    // Make entries_num exactly divisible m_num_hashes
    if (entries_num % iblt_params.num_hashes != 0) {
      entries_num = iblt_params.num_hashes * ((entries_num / iblt_params.num_hashes) + 1);
    }

    return entries_num;
  }

  class HashTableEntry {
   public:
    TCount count = 0;
    TKey key_sum = 0;
    uint32_t key_check = 0;
    std::vector<uint8_t> value_sum;

    bool IsPure() const {
      if (count == 1 || count == -1) {
        uint32_t check = MurmurHash3(N_HASHCHECK, ToVec(key_sum));
        return (key_check == check);
      }
      return false;
    }

    bool Empty() const {
      return (count == 0 && key_sum == 0 && key_check == 0);
    }

    void AddValue(const std::vector<uint8_t> &v) {
      if (v.empty()) {
        return;
      }

      if (value_sum.size() < v.size()) {
        value_sum.resize(v.size());
      }

      for (size_t i = 0; i < v.size(); i++) {
        value_sum[i] ^= v[i];
      }
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream &s, Operation ser_action) {
      READWRITE(count);
      READWRITE(key_sum);
      READWRITE(key_check);
      if (ValueSize != 0) {
        READWRITE(value_sum);
      }
    }
  };

 private:
  static constexpr size_t N_HASHCHECK = 11;

  void _insert(TCount plusOrMinus, TKey k, const std::vector<uint8_t> &v) {
    assert(v.size() == ValueSize);

    // UNIT-E TODO: this is very inefficient to allocate memory just to compute murmurhash!
    std::vector<uint8_t> kvec = ToVec(k);
    const auto key_check = MurmurHash3(N_HASHCHECK, kvec);

    size_t bucketsPerHash = m_hash_table.size() / m_num_hashes;
    for (size_t i = 0; i < m_num_hashes; i++) {
      size_t startEntry = i * bucketsPerHash;

      uint32_t h = MurmurHash3(i, kvec);
      const auto bucket_n = startEntry + (h % bucketsPerHash);
      IBLT::HashTableEntry &entry = m_hash_table.at(bucket_n);
      entry.count += plusOrMinus;
      entry.key_sum ^= k;
      entry.key_check ^= key_check;
      if (entry.Empty()) {
        entry.value_sum.clear();
      } else {
        entry.AddValue(v);
      }
    }
  }

  std::vector<HashTableEntry> m_hash_table;
  uint8_t m_num_hashes = 0;
};

#endif /* UNITE_IBLT_H */
