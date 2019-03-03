// Copyright (c) 2014 Gavin Andresen
// Copyright (c) 2019 The Unit-e developers
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

//! \brief Invertible Bloom Lookup Table implementation
//!
//! References:
//!
//! "What's the Difference? Efficient Set Reconciliation
//! without Prior Context" by Eppstein, Goodrich, Uyeda and
//! Varghese
//!
//! "Invertible Bloom Lookup Tables" by Goodrich and
//! Mitzenmacher
template <typename TKey, size_t ValueSize>
class IBLT {
 public:
  using TEntriesMap = std::map<TKey, std::vector<uint8_t>>;

  explicit IBLT(size_t expected_items_count) {
    const IBLTParams optimal_params = IBLTParams::FindOptimal(expected_items_count);
    m_num_hashes = optimal_params.num_hashes;

    const size_t entries_num = ComputeEntriesNum(expected_items_count, optimal_params);

    m_hash_table.resize(entries_num);
  }

  IBLT() = default;

  IBLT(const size_t num_entries, const uint8_t num_hashes)
      : m_hash_table(num_entries), m_num_hashes(num_hashes) {
    assert(IsValid());
  }

  void Insert(const TKey key, const std::vector<uint8_t> &value) {
    Update(1, key, value);
  }

  void Erase(const TKey key, const std::vector<uint8_t> &value) {
    Update(-1, key, value);
  }

  //! \brief Tries to get a value from the IBLT
  //!
  //! \returns True if a result is definitely found or not
  //! found. If not found, \p value_out will be empty.
  //! returns false if overloaded and we don't know whether or
  //! not key is in the table
  bool Get(const TKey key, std::vector<uint8_t> &value_out) const {
    value_out.clear();

    const size_t buckets_per_hash = m_hash_table.size() / m_num_hashes;
    for (size_t i = 0; i < m_num_hashes; i++) {
      const size_t start_entry = i * buckets_per_hash;

      // Although in theory seed might overflow here - we don't care.
      // It is seed after all
      const auto seed = static_cast<unsigned int>(i);
      const unsigned int h = ComputeHash(seed, key);
      const size_t bucket = start_entry + (h % buckets_per_hash);
      const IBLTEntry &entry = m_hash_table.at(bucket);

      if (entry.IsEmpty()) {
        // Definitely not in the table. Leave result empty, return true.
        return true;
      }

      if (entry.IsPure()) {
        if (entry.key_sum == key) {
          // Found!
          value_out = entry.value_sum;
        }
        // Otherwise - definitely not in the table.
        // In any case - we are confident about result, so return true
        return true;
      }
    }

    // Don't know if key is in table or not; "peel" the IBLT to try to find it
    IBLT<TKey, ValueSize> peeled = *this;
    bool erased = false;
    for (size_t i = 0; i < peeled.m_hash_table.size(); ++i) {
      const IBLTEntry &entry = peeled.m_hash_table[i];
      if (entry.IsPure()) {
        if (entry.key_sum == key) {
          // Found!
          value_out = entry.value_sum;
          return true;
        }
        erased = true;

        // Need a copy because `Update` will reiterate table and might change
        // our entry because it is a reference
        const std::vector<uint8_t> value_sum_copy = entry.value_sum;
        peeled.Update(-entry.count, entry.key_sum, value_sum_copy);
      }
    }

    if (erased) {
      // Recurse with smaller IBLT
      return peeled.Get(key, value_out);
    }
    return false;
  }

  //! \brief Decodes IBLT entries
  //!
  //! Adds entries to the given sets:
  //! \param positive_out All entries that were inserted
  //! \param negative_out All entries that were erased but never added (or if
  //! the IBLT = A-B, all entries in B that are not in A)
  //! \returns True if all entries could be decoded, false otherwise
  bool ListEntries(TEntriesMap &positive_out,
                   TEntriesMap &negative_out) const {
    IBLT<TKey, ValueSize> peeled = *this;

    bool erased;
    do {
      erased = false;
      for (size_t i = 0; i < peeled.m_hash_table.size(); ++i) {
        const IBLTEntry &entry = peeled.m_hash_table[i];
        if (entry.IsPure()) {
          if (entry.count == 1) {
            positive_out.emplace(entry.key_sum, entry.value_sum);
          } else {
            negative_out.emplace(entry.key_sum, entry.value_sum);
          }
          // Update will reiterate table and might change our entry because it
          // is a reference
          const std::vector<uint8_t> copy = entry.value_sum;
          peeled.Update(-entry.count, entry.key_sum, copy);
          erased = true;
        }
      }
    } while (erased);

    // If any buckets for one of the hash functions is not empty,
    // then we didn't peel them all:
    for (size_t i = 0; i < peeled.m_hash_table.size() / m_num_hashes; ++i) {
      if (!peeled.m_hash_table[i].IsEmpty()) {
        return false;
      }
    }
    return true;
  }

  //! \brief Subtract two IBLTs
  IBLT<TKey, ValueSize> operator-(const IBLT<TKey, ValueSize> &other) const {
    // IBLT's must be same params/size:
    assert(m_hash_table.size() == other.m_hash_table.size());
    assert(m_num_hashes == other.m_num_hashes);

    IBLT<TKey, ValueSize> result(*this);
    for (size_t i = 0; i < m_hash_table.size(); ++i) {
      IBLTEntry &e1 = result.m_hash_table[i];
      const IBLTEntry &e2 = other.m_hash_table[i];
      e1.count -= e2.count;
      e1.key_sum ^= e2.key_sum;
      e1.key_check ^= e2.key_check;
      if (e1.IsEmpty()) {
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

  //! \brief Returns how many items were inserted
  size_t Size() const {
    assert(IsValid());

    size_t sum = 0;
    for (const IBLTEntry &entry : m_hash_table) {
      sum += llabs(entry.count);
    }

    return sum / m_num_hashes;
  }

  //! \brief Makes new empty IBLT instance with parameters equal to this
  IBLT<TKey, ValueSize> CloneEmpty() const {
    return IBLT<TKey, ValueSize>(m_hash_table.size(), m_num_hashes);
  }

  //! \brief Checks if IBLT parameters are within acceptable limits
  //!
  //! When we are creating new IBLT - we can adjust those values to whatever we
  //! need, but if we receive them from network - they must meet these criteria
  bool IsValid() const {
    if (m_num_hashes == 0) {
      return false;
    }

    return m_hash_table.size() % m_num_hashes == 0;
  }

  //! \brief Computes exact number of entries without creating an IBLT
  static size_t ComputeEntriesNum(size_t expected_items_count,
                                  boost::optional<IBLTParams> params = {}) {
    const IBLTParams iblt_params = params
                                       ? params.get()
                                       : IBLTParams::FindOptimal(expected_items_count);

    auto entries_num = static_cast<size_t>(iblt_params.overhead * expected_items_count);

    // Make entries_num exactly divisible by num_hashes
    if (entries_num % iblt_params.num_hashes != 0) {
      entries_num = iblt_params.num_hashes * ((entries_num / iblt_params.num_hashes) + 1);
    }

    return entries_num;
  }

  class IBLTEntry {
   public:
    int64_t count = 0;
    TKey key_sum = 0;
    uint32_t key_check = 0;
    std::vector<uint8_t> value_sum;

    bool IsPure() const {
      if (count == 1 || count == -1) {
        const unsigned int check = ComputeHash(N_HASHCHECK, key_sum);
        return key_check == check;
      }
      return false;
    }

    bool IsEmpty() const {
      return count == 0 && key_sum == 0 && key_check == 0;
    }

    void AddValue(const std::vector<uint8_t> &value) {
      if (value.empty()) {
        return;
      }

      if (value_sum.size() < value.size()) {
        value_sum.resize(value.size());
      }

      for (size_t i = 0; i < value.size(); i++) {
        value_sum[i] ^= value[i];
      }
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    void SerializationOp(Stream &s, Operation ser_action) {
      assert(count >= 0 && "Current IBLT implementation does not support negative values serialization");
      auto unsigned_count = static_cast<uint64_t>(count);
      READWRITE(COMPACTSIZE(unsigned_count));
      if (ser_action.ForRead()) {
        count = static_cast<int64_t>(unsigned_count);
      }

      READWRITE(key_sum);
      READWRITE(key_check);
      if (ValueSize != 0) {
        READWRITE(value_sum);
      }
    }
  };

 private:
  static unsigned int ComputeHash(unsigned int seed, const TKey &key) {
    static_assert(std::is_integral<TKey>::value, "Only integral keys are supported");
    const auto data_ptr = reinterpret_cast<const uint8_t *>(&key);

    return MurmurHash3(seed, data_ptr, sizeof(key));
  }

  static constexpr size_t N_HASHCHECK = 11;

  void Update(const int64_t count_delta,
              const TKey key,
              const std::vector<uint8_t> &value) {

    assert(value.size() == ValueSize);

    const unsigned int key_check = ComputeHash(N_HASHCHECK, key);

    const size_t buckets_per_hash = m_hash_table.size() / m_num_hashes;
    for (size_t i = 0; i < m_num_hashes; i++) {
      const size_t start_entry = i * buckets_per_hash;

      // Although in theory seed might overflow here - we don't care.
      // It is seed after all
      const auto seed = static_cast<unsigned int>(i);
      const unsigned int h = ComputeHash(seed, key);
      const size_t bucket = start_entry + (h % buckets_per_hash);
      IBLTEntry &entry = m_hash_table.at(bucket);
      entry.count += count_delta;
      entry.key_sum ^= key;
      entry.key_check ^= key_check;
      if (entry.IsEmpty()) {
        entry.value_sum.clear();
      } else {
        entry.AddValue(value);
      }
    }
  }

  std::vector<IBLTEntry> m_hash_table;
  uint8_t m_num_hashes = 0;
};

#endif /* UNITE_IBLT_H */
