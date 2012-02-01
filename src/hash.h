// hash.h
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
// Copyright 2010 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Hash functions (based on boost::hash) and functors for hash_map, hash_set

#ifndef HASH_H_
#define HASH_H_

#include <cstddef>

namespace trainc {

template<class T>
inline void HashCombine(size_t &seed, T const &v) {
  seed ^= v + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

template<class Iterator>
inline size_t HashRange(Iterator begin, Iterator end, size_t seed) {
  for (; begin != end; ++begin) {
    HashCombine(seed, *begin);
  }
  return seed;
}

// Hash functor for objects with a HashValue() method.
template<class T>
class Hash {
 public:
  size_t operator()(const T &s) const {
    return s.HashValue();
  }
};

// Equality functor for objects with an IsEqual() method.
template<class T>
class Equal {
 public:
  bool operator()(const T &a, const T &b) const {
    return a.IsEqual(b);
  }
};

}  // namespace trainc

#endif  // HASH_H_
