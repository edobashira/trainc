// transducer_check.h
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
// Definition of ConstructionalTransducerCheck, which checks
// a ConstructionalTransducer to have a valid structure, and
// CTranducerCheck, which checks that the generated C transducer
// accepts all phone sequences.

#ifndef TRANSDUCER_CHECK_H_
#define TRANSDUCER_CHECK_H_

#include <string>
#include <vector>
#include "fst/symbol-table.h"
#include "fst/vector-fst.h"
#include "context_set.h"
#include "stringmap.h"


namespace trainc {

class ConstructionalTransducer;
class State;
class Arc;
class Phones;

// Checks the validity of a ConstructionalTransducer.
// The following properties are tested:
//  - deterministic output
//  - context of phone models (input label) matches states
//  - histories of state sequences match
class ConstructionalTransducerCheck {
 public:
  ConstructionalTransducerCheck(const ConstructionalTransducer &c,
                  const Phones *phone_info,
                  int num_left_contexts, int num_right_contexts);
  ~ConstructionalTransducerCheck() {}
  bool IsValid() const;

 private:
  bool CheckDeterministicOutput(const State &state) const;
  bool CheckPhoneModel(const State &state, const Arc &arc) const;
  bool CheckStateModelCompatibility(const State &state, const Arc &arc) const;
  bool CheckStateModels(const State &state, const Arc &arc) const;
  bool CheckTargetState(const State &state, const Arc &arc) const;
  bool IsCiPhoneState(const State &state) const;
  const ConstructionalTransducer &c_;
  const Phones *phone_info_;
  int num_left_contexts_, num_right_contexts_;
  ContextSet all_phones_;
};


class PhoneSequenceIterator;


// Checks the validity of a context dependency transducer by
// enumerating all possible phone sequences, composing them with the
// C transducer and validating the input of the composed transducer.
class CTransducerCheck {
 public:
  CTransducerCheck() : phones_(NULL), hmms_(0), length_(0), c_(NULL) {}
  ~CTransducerCheck();

  // Initialize symbol tables for phone symbols and hmm symbols.
  // hmm_to_phone: mapping from context dependent HMM symbols to
  //               phone symbols.
  // boundary_phone: phone occuring at the start and end of a sequence.
  // context_length: sum of left and right contexts + 1.
  void Init(const std::string &phone_symbols, const std::string &hmm_symbols,
            const std::string &hmm_to_phone, const std::string &boundary_phone,
            int context_length);

  // Set the transducer to be validated.
  void SetTransducer(const fst::StdVectorFst *c);

  // Perform the check and return true if the transducer
  // passes all tests.
  bool IsValid() const;

 private:
  void AddBoundaryPhone(fst::StdVectorFst *phone_fst) const;
  bool CheckPhoneSequence(const fst::StdVectorFst &phone_fst,
                          const vector<int> &phone_seq) const;
  void PrintSequence(const fst::StdVectorFst &cl) const;
  void GetSequence(const fst::StdVectorFst &cl, vector<int> *hmm_seq) const;
  fst::SymbolTable *phones_, *hmms_;
  int length_, boundary_phone_;
  const fst::StdVectorFst *c_;
  StringMap hmm_to_phone_;
  PhoneSequenceIterator *piter_;
};


}  // namespace trainc

#endif  // TRANSDUCER_CHECK_H_
