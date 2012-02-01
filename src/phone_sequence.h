// phone_sequence.h
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
//
// \file
// N-phone iterator


#ifndef PHONE_SEQUENCE_H_
#define PHONE_SEQUENCE_H_

#include <string>
#include <vector>
#include "fst/symbol-table.h"
#include "fst/vector-fst.h"

namespace trainc {

// Iterator over all n-phones.
class PhoneSequenceIterator {
 public:
  // setup iterator for n-phones of the given length using all
  // values from the given symbol table (except 0).
  PhoneSequenceIterator(int length, const fst::SymbolTable *symbols);
  ~PhoneSequenceIterator();

  // length of the phone sequences
  int length() const { return length_; }

  // reset the iterator
  void Reset();

  // iteration completed
  bool Done() const { return iterators_.back()->Done(); }

  // increments the iterator.
  void Next(void);

  // returns the current n-phone as a vector of symbol strings.
  void StringValue(std::vector<std::string> *strings) const;

  // returns the current n-phone as vector of index values
  void IndexValue(std::vector<int> *indexes) const;

  // returns the current n-phone as sequential transducer.
  void TransducerValue(fst::StdVectorFst *fst) const;

 private:
  typedef std::vector<fst::SymbolTableIterator*> IteratorVector;
  void SkipEpsilon(fst::SymbolTableIterator *i) const;

  int length_;
  const fst::SymbolTable *symbols_;
  IteratorVector iterators_;
};

}  // namespace trainc

#endif  // PHONE_SEQUENCE_H_
