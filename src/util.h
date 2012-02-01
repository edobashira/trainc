// util.h
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
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Utility inline definitions

#ifndef UTIL_H_
#define UTIL_H_

#include "fst/compat.h"
#include <algorithm>
#include <list>
#include <utility>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

namespace trainc {

// Utility to access a pair using an index.
template<class T>
inline T& GetPairElement(std::pair<T, T> &p, bool i) {
  return (i ? p.second : p.first);
}

// Utility to access a const pair using an index.
template<class T>
inline const T& GetPairElement(const std::pair<T, T> &p, bool i) {
  return (i ? p.second : p.first);
}

// Utility functor to convert a pointer to a hash value
template<class T>
class PointerHash {
public:
  size_t operator()(const T* const &ptr) const {
    return reinterpret_cast<size_t>(ptr);
  }
};

// Utility functor to get the memory address of an object
template<class T>
class GetAddress {
public:
  const T* operator()(const T &o) const {
    return &o;
  }
};

// Hash functor for List iterators
template<class T>
class ListIteratorHash {
 public:
  size_t operator()(const typename std::list<T>::iterator &i) const {
    return reinterpret_cast<size_t>(i._M_node);
  }
};

// Comparison functor for List iterators
template<class T>
class ListIteratorCompare {
 public:
  bool operator()(const typename std::list<T>::iterator &a,
                  const typename std::list<T>::iterator &b) const {
    return a._M_node < b._M_node;
  }
};

// copy all elements of an input iterator with Next(), Done() methods
// to the given OutputIterator.
// works like std::copy but with Next()/Done() iterators
template<class InputIterator, class OutputIterator>
void Copy(InputIterator *iter, OutputIterator oiter) {
  for (; !iter->Done(); iter->Next(), ++oiter) {
    oiter = iter->Value();
  }
}

// remove duplicates from the given vector.
// sorts the elements of the vector.
template<class T>
void RemoveDuplicates(std::vector<T> *v) {
  std::sort(v->begin(), v->end());
  v->erase(std::unique(v->begin(), v->end()), v->end());
}

template<class T, class C>
void RemoveDuplicates(std::vector<T> *v, C compare) {
  std::sort(v->begin(), v->end(), compare);
  v->erase(std::unique(v->begin(), v->end()), v->end());
}


}  // namespace trainc

// Apply delete to all elements of an STL style container
template<class T>
inline void STLDeleteElements(T *container) {
  for (typename T::iterator i = container->begin(); i != container->end(); ++i)
    delete *i;
}

template<class Iter>
inline void STLDeleteContainerPairSecondPointers(Iter begin, Iter end) {
  for (; begin != end; ++begin)
    delete begin->second;
}


// Google-style assertions
// some of the CHECK_ macros are incorrect in earlier OpenFst version
#undef CHECK
#undef CHECK_LE
#undef CHECK_LT
#undef CHECK_GE
#undef CHECK_GT
#undef CHECK_NE
#undef CHECK_EQ
#undef CHECK_NOTNULL

#ifdef DEBUG
namespace {

void stackTrace()
{
#ifdef HAVE_EXECINFO_H
  const size_t kMaxTraces = 100;
  void *buffer[kMaxTraces];
  size_t num_traces = ::backtrace(buffer, kMaxTraces);
  char **symbols = ::backtrace_symbols(buffer, num_traces);
  for (size_t i = 2; i < num_traces; i++) {
    // skip current function
    std::cerr << '#' << i << "  " << symbols[i] << std::endl;
  }
  free(symbols);
#endif  // HAVE_EXECINFO_H
}

template<class S, class T>
void AssertionFailed(const char *expr, const S &x, const T &y,
                     const char *file, int line, const char *func) {
  std::cerr << "assertion '" << expr << "' failed: "
            << "$1=" << x << " $2=" << y << "\n"
            << file << ":" << line << " " << func << std::endl;
  stackTrace();
  std::exit(EXIT_FAILURE);
}
}  // namespace

#define ASSERT(expr, a, b) \
  (expr) ? static_cast<void>(0) : \
      AssertionFailed(#expr, a, b, \
                      __FILE__, __LINE__, __PRETTY_FUNCTION__)
#else
#define ASSERT(expr, a, b) assert(expr)
#endif  // DEBUG

#define CHECK(x) ASSERT((x), x, true)
#define CHECK_LE(x, y) ASSERT((x) <= (y), x, y)
#define CHECK_LT(x, y) ASSERT((x) < (y), x, y)
#define CHECK_GE(x, y) ASSERT((x) >= (y), x, y)
#define CHECK_GT(x, y) ASSERT((x) > (y), x, y)
#define CHECK_NE(x, y) ASSERT((x) != (y), x, y)
#define CHECK_EQ(x, y) ASSERT((x) == (y), x, y)
#define CHECK_NOTNULL(x) ASSERT(x != NULL, x, NULL)

// Google-style logging
#define REP(type) LOG(type)

#define VLOG_IS_ON(level) ((level) <= FLAGS_v)

#endif  // UTIL_H_


