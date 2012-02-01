// shifted_init.h
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
// Copyright 2012 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Initialization of a shifted LexiconTransducer

#ifndef SHIFTED_INIT_H_
#define SHIFTED_INIT_H_

#include "fst/vector-fst.h"
#include "lexicon_init.h"

namespace trainc {

class ShiftedLexiconTransducerInitializer: public LexiconTransducerInitializer {
public:
  typedef map<int, int> PhoneMapping;

  ShiftedLexiconTransducerInitializer(LexiconTransducer *target)
    : LexiconTransducerInitializer(target), boundary_phone_(-1) {}
  void SetPhoneMapping(const PhoneMapping &map) {
    phone_mapping_ = map;
  }
  void SetBoundaryPhone(int boundary_phone) {
    boundary_phone_ = boundary_phone;
  }
  virtual void Build(const fst::StdExpandedFst &l);
protected:
  void Prepare(const fst::StdExpandedFst &l, fst::StdVectorFst *t) const;
  PhoneMapping phone_mapping_;
  int boundary_phone_;
};

}  // namespace trainc

#endif  // SHIFTED_INIT_H_
