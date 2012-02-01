// phone_sequence.cc
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
// Copyright 2011 Google Inc. All Rights reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)

#include "phone_sequence.h"
#include "util.h"

namespace trainc {

PhoneSequenceIterator::PhoneSequenceIterator(int length,
                                             const fst::SymbolTable *symbols) :
    length_(length), symbols_(symbols),
    iterators_(length_)
{
  for (IteratorVector::iterator i = iterators_.begin();
      i != iterators_.end(); ++i) {
    *i = new fst::SymbolTableIterator(*symbols);
    SkipEpsilon(*i);
  }
}

PhoneSequenceIterator::~PhoneSequenceIterator() {
  STLDeleteElements(&iterators_);
}

void PhoneSequenceIterator::Reset() {
  for (IteratorVector::iterator i = iterators_.begin();
      i != iterators_.end(); ++i) {
    (*i)->Reset();
    SkipEpsilon(*i);
  }
}

void PhoneSequenceIterator::Next() {
  if (Done()) return;
  for (IteratorVector::iterator i = iterators_.begin();
      i != iterators_.end(); ++i) {
    (*i)->Next();
    SkipEpsilon(*i);
    if (!(*i)->Done() || *i == iterators_.back())
      break;
    (*i)->Reset();
    SkipEpsilon(*i);
  }
}

void PhoneSequenceIterator::StringValue(
    std::vector<std::string> *strings) const {
  strings->clear();
  for (IteratorVector::const_iterator i = iterators_.begin();
      i != iterators_.end(); ++i)
    strings->push_back((*i)->Symbol());
}

void PhoneSequenceIterator::IndexValue(std::vector<int> *indexes) const {
  indexes->clear();
  for (IteratorVector::const_iterator i = iterators_.begin();
      i != iterators_.end(); ++i)
    indexes->push_back((*i)->Value());
}

void PhoneSequenceIterator::TransducerValue(fst::StdVectorFst *fst) const {
  fst->DeleteStates();
  fst::StdArc::StateId state = fst->AddState();
  fst->SetStart(state);
  for (IteratorVector::const_iterator i = iterators_.begin();
      i != iterators_.end(); ++i) {
    fst::StdArc::StateId next_state = fst->AddState();
    int label = (*i)->Value();
    fst->AddArc(state, fst::StdArc(label, label, fst::StdArc::Weight::One(),
                                   next_state));
    state = next_state;
  }
  fst->SetFinal(state, fst::StdArc::Weight::One());
}

inline void PhoneSequenceIterator::SkipEpsilon(
    fst::SymbolTableIterator *i) const {
  while (!i->Done() && i->Value() == 0) i->Next();
}

}  // namespace trainc {
