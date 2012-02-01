// integer_set.h
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright 2010 Google Inc. All Rights Reserved.
// Author: rybach@google.com (David Rybach)

#ifndef INTEGER_SET_H_
#define INTEGER_SET_H_

#include <algorithm>
#include <string>
#include <vector>
#include "array.h"
#include "debug.h"
#include "hash.h"
#include "util.h"

namespace trainc {

template<class T, size_t N> class IntegerSetIterator;


// Set of unsigned integers within a limited range.
// The maximum number of elements, i.e. the highest value, has to be
// defined at construction and cannot be changed.
// T: underlying storage unit for bits.
// max_elements: maximum number of elements.
template<class T = uint64, size_t max_elements = 256>
class IntegerSet {
  typedef T Word;
 public:
  typedef unsigned int ValueType;
  typedef IntegerSetIterator<T, max_elements> Iterator;

  explicit IntegerSet(size_t capacity)
      : num_bits_(capacity),
        num_words_((capacity + (kBitsPerWord - 1)) / kBitsPerWord),
        words_(num_words_, 0) {
    CHECK_LE(capacity, max_elements);
  }

  // Maximum number of items in the set.
  size_t Capacity() const {
    return num_bits_;
  }

  // Returns the maximum set size.
  static size_t MaxCapacity() {
    return max_elements;
  }

  // Number of elements.
  // bit counting using Brian Kernighan's way.
  size_t Size() const {
    const Word* bits = words_.array();
    size_t count = 0;
    Word w;
    for (int i = 0; i < num_words_; ++i, ++bits) {
      for (w = *bits; w; ++count)
        w &= w - 1; // clear the least significant bit set
    }
    return count;
  }

  // Is element a member of the set.
  bool HasElement(ValueType element) const {
    return GetBit(element);
  }

  // Add a element to the set.
  void Add(ValueType element) {
    SetBit(element);
  }

  // Remove a element from the set.
  void Remove(ValueType element) {
    ClearBit(element);
  }

  // Add a range of elements to the set.
  template<class InputIterator>
  void AddElements(InputIterator begin, InputIterator end) {
    for (; begin != end; ++begin)
      Add(*begin);
  }

  // Replace the set with its intersection with the set c.
  void Intersect(const IntegerSet &c) {
    DCHECK_EQ(Capacity(), c.Capacity());
    Word *a = words_.mutable_array();
    const Word *o = c.words_.array();
    for (int i = 0; i < num_words_; ++i) {
      a[i] &= o[i];
    }
  }

  // Replace the set with its union with the set c.
  void Union(const IntegerSet &c) {
    DCHECK_EQ(Capacity(), c.Capacity());
    Word *a = words_.mutable_array();
    const Word *o = c.words_.array();
    for (int i = 0; i < num_words_; ++i) {
      a[i] |= o[i];
    }
  }

  // Returns true if the set does not contain any item.
  bool IsEmpty() const {
    const Word* bits = words_.array();
    for (int i = 0; i < num_words_; ++i, ++bits) {
      if (*bits) return false;
    }
    return true;
  }

  // Returns true if both sets contain the same elements.
  bool IsEqual(const IntegerSet &other) const {
    DCHECK_EQ(Capacity(), other.Capacity());
    const Word* a = words_.array();
    const Word* o = other.words_.array();
    for (int i = 0; i < num_words_; ++i) {
      if (a[i] != o[i])
        return false;
    }
    return true;
  }

  // Returns true if this set is a subset of the given set super_set.
  bool IsSubSet(const IntegerSet &super_set) const {
    DCHECK_EQ(Capacity(), super_set.Capacity());
    if (IsEmpty())
      return true;
    const Word* my_bits = words_.array();
    const Word* ss_bits = super_set.words_.array();
    for (int i = 0; i < num_words_; ++i) {
      const Word &m = my_bits[i];
      const Word &s = ss_bits[i];
      if (m && (!(m & s) || (m & ~s)))
        return false;
    }
    return true;
  }

  // Replace the set by its complement.
  void Invert() {
    Word* a = words_.mutable_array();
    for (int i = 0; i < num_words_; ++i) {
      a[i] = ~a[i];
    }
    // set unused bits to zero
    Word clear_unused_bits = static_cast<Word>(-1)
        >> (-num_bits_ & (kBitsPerWord - 1));
    a[num_words_ - 1] &= clear_unused_bits;
  }

  // Reset to empty set
  void Clear() {
    Word* a = words_.mutable_array();
    for (int i = 0; i < num_words_; ++i) {
      a[i] = static_cast<Word>(0);
    }
  }

  // Computes a hash value for the set.
  size_t HashValue() const {
    const Word* a = words_.array();
    return HashRange(a, a + num_words_, 0);
  }

 protected:
  bool GetBit(size_t position) const {
    DCHECK_LT(position, num_bits_);
    const Word *a = words_.array();
    return (a[GetWordIndex(position)] &
        (static_cast<Word>(1) << GetBitIndex(position)));
  }

  void SetBit(size_t position) {
    DCHECK_LT(position, num_bits_);
    Word *a = words_.mutable_array();
    a[GetWordIndex(position)] |=
        (static_cast<Word>(1) << GetBitIndex(position));
  }

  void ClearBit(size_t position) {
    DCHECK_LT(position, num_bits_);
    Word *a = words_.mutable_array();
    a[GetWordIndex(position)] &=
        ~(static_cast<Word>(1) << GetBitIndex(position));
  }

  size_t GetWordIndex(size_t position) const {
    DCHECK_LT(position, num_bits_);
    return (position / kBitsPerWord);
  }

  size_t GetBitIndex(size_t position) const {
    DCHECK_LT(position, num_bits_);
    return (position & (kBitsPerWord - 1));
  }

 private:
  // number of bits in the used int type
  enum { kBitsPerWord = sizeof(Word) * 8 };
  size_t num_bits_, num_words_;
  Array<Word, (max_elements + kBitsPerWord - 1) / kBitsPerWord> words_;
};


// Iterator for an IntegerSet.
template<class T, size_t N>
class IntegerSetIterator {
 public:
  IntegerSetIterator(const IntegerSet<T, N> &set)
      : set_(set), element_(0) {
    FindNext();
  }

  bool Done() const {
    return element_ >= set_.Capacity();
  }

  void Next() {
    ++element_;
    FindNext();
  }

  typename IntegerSet<T, N>::ValueType Value() const {
    return element_;
  }

 private:
  void FindNext() {
    while (!Done() && !set_.HasElement(element_))
      ++element_;
  }
  const IntegerSet<T, N> &set_;
  typename IntegerSet<T, N>::ValueType element_;
};

}  // namespace trainc

#endif  // INTEGER_SET_H_
