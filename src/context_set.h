// context_set.h
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

#ifndef CONTEXT_SET_H_
#define CONTEXT_SET_H_

#include <string>
#include <utility>
#include <vector>
#include "integer_set.h"
#include "util.h"
#include <sstream>
using std::vector;

namespace trainc {

// Maximum number of phone symbols supported by this module.
enum { kMaxNumPhones = 256 };
typedef IntegerSet<uint64, kMaxNumPhones> ContextSet;

// DEBUG
inline std::string ContextSetToString(const ContextSet &c) {
  std::stringstream ss;
  for (ContextSet::Iterator p(c); !p.Done(); p.Next())
    ss << p.Value() << " ";
  return ss.str();
}

// A pair of two context sets.
// Implementation using std::pair<const ContextSet&, const ContextSet&>
// fails with GCC 4.{1,2} because of the "reference to reference" problem
// in the constructor of std::pair.
struct Partition {
public:
  Partition(const ContextSet &a, const ContextSet &b) : first(a), second(b) {}
  explicit Partition(const std::pair<const ContextSet, const ContextSet> &p)
      : first(p.first), second(p.second) {}
  const ContextSet &first;
  const ContextSet &second;
};

// Overloading of GetPairElement for Partition
inline const ContextSet& GetPairElement(const Partition &p, bool i) {
  return (i ? p.second : p.first);
}


// The left and right context of a CD unit, which may consist of
// several phones.
// Each context position is a set of allowed / equivalent
// phones at that position.
// Context position is negative for left contexts and positive for right
// contexts, with increasing absolute value starting from the center.
// Context position 0 is the set of phones represented by the unit.
// For a pentaphone (A B)_C_(D E)
//  position -1 = B, position -2 = A
//  position  1 = D, position  2 = E
//  position  0 = C
class PhoneContext {
 public:
  // Initializes all context sets with an empty set.
  // num_phones is required for the initialization of ContextSet.
  PhoneContext(
      int num_phones, int num_left_contexts, int num_right_contexts)
      : num_left_contexts_(num_left_contexts),
        contexts_(num_left_contexts + num_right_contexts + 1,
                  ContextSet(num_phones)) {}

  // Number of contexts to the left.
  int NumLeftContexts() const {
    return num_left_contexts_;
  }

  // Number of contexts to the right.
  int NumRightContexts() const {
    return contexts_.size() - num_left_contexts_ - 1;
  }

  // Context at the given position.
  const ContextSet& GetContext(int position) const {
    return contexts_[ContextPositionToIndex(position)];
  }

  // Mutable access the the ContextSet for the given position.
  ContextSet* GetContextRef(int position) {
    return &contexts_[ContextPositionToIndex(position)];
  }

  // Set the context set of the given position.
  void SetContext(int position, const ContextSet &c) {
    contexts_[ContextPositionToIndex(position)] = c;
  }

  // Returns true if all ContextSets of this PhoneContext and the
  // the given PhoneContexts are equal.
  bool IsEqual(const PhoneContext &other) const;

  // Hash value.
  size_t HashValue() const;

  // String representation.
  string ToString() const;

 private:
  // transform context position to an index in contexts_.
  // position is negative for left contexts and
  // positive for right contexts.
  size_t ContextPositionToIndex(int position) const;

  size_t num_left_contexts_;
  vector<ContextSet> contexts_;
};

inline bool PhoneContext::IsEqual(const PhoneContext &other) const {
  DCHECK_EQ(contexts_.size(), other.contexts_.size());
  vector<ContextSet>::const_iterator c = contexts_.begin(),
      o = other.contexts_.begin();
  for (; c != contexts_.end(); ++c, ++o)
    if (!c->IsEqual(*o)) return false;
  return true;
}

inline size_t PhoneContext::HashValue() const {
  // TODO(rybach): improve hash function
  DCHECK(!contexts_.empty());
  size_t h = contexts_.front().HashValue();
  for (vector<ContextSet>::const_iterator c = contexts_.begin() + 1;
      c != contexts_.end(); ++c) {
    HashCombine(h, c->HashValue());
  }
  return h;
}

inline size_t PhoneContext::ContextPositionToIndex(int position) const {
  size_t idx = 0;
  if (position < 0) {
    idx = -(position + 1);
  } else {
    idx = num_left_contexts_ + position;
  }
  DCHECK_LT(idx, contexts_.size());
  return idx;
}


// Definition of a partitioning of a set of phones into two disjunct sets.
class ContextQuestion {
 public:
  // Initialize the partition with the given set of phones and its complement.
  explicit ContextQuestion(const ContextSet &question)
    : yes_(question), no_(question) {
    no_.Invert();
  }

  ContextQuestion(const ContextSet &question, const std::string &name)
    : yes_(question), no_(question), name_(name) {
    no_.Invert();
  }


  // Phone set for partition 0/1.
  const ContextSet& GetPhoneSet(bool b) const {
    return (b ? no_ : yes_);
  }

  const std::string& Name() const {
    return name_;
  }

 private:
  // sets to separate a phone set with.
  ContextSet yes_, no_;
  const std::string name_;
  DISALLOW_COPY_AND_ASSIGN(ContextQuestion);
};

}  // namespace trainc

#endif  // CONTEXT_SET_H_
