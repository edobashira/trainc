// transducer_init.h
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
// Classes to initialize the phone models, state models, and the
// ConstructionalTransducer.


#ifndef TRANSDUCER_INIT_H_
#define TRANSDUCER_INIT_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "context_set.h"

using std::list;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace trainc {

class AllophoneModel;
class ConstructionalTransducer;
class ModelManager;
class PhoneContext;
class Phones;
class State;

// Abstract base class for classes that populate a ConstructionalTransducer
// with an initial set of states and arcs and creates the matching
// AllophoneModels.
// After setting up the data members, the methods are expected to be
// called in the following order:
//   Prepare();
//   CreateModels();
//   Execute();
class TransducerInitialization {
 public:

  TransducerInitialization() : phone_info_(NULL) {}
  virtual ~TransducerInitialization() {}

  // Set the phone informations
  void SetPhoneInfo(const Phones *phone_info) {
    phone_info_ = phone_info;
  }

  // Set the number of contexts to the left and the right.
  void SetContextLenghts(int left, int right) {
    num_left_contexts_ = left;
    num_right_contexts_ = right;
  }

  // Set the phone set containing all phones
  void SetAnyPhoneContext(const ContextSet *any_phone) {
    any_phone_ = any_phone;
  }

  // Prepare the Initialization.
  virtual bool Prepare() {
    return true;
  }

  // Create the required models for the input labels.
  virtual void CreateModels(ModelManager *models) = 0;

  // Perform the initialization of the transducer.
  virtual void Execute(ConstructionalTransducer *t) = 0;

 protected:
  int num_phones_;
  int num_left_contexts_, num_right_contexts_;
  const Phones *phone_info_;
  const ContextSet *any_phone_;
};


// Initializes the transducer with one state per phone,
// a monophone model for each phone,
// and all valid arcs between the states.
class BasicTransducerInitialization : public TransducerInitialization {
 public:
  virtual ~BasicTransducerInitialization() {}
  virtual void CreateModels(ModelManager *models);
  virtual void Execute(ConstructionalTransducer *t);

 protected:
  virtual void CreateUnits(int num_phones);
  virtual void SetUnitHistory(int phone, PhoneContext *history) const;
  virtual void CreateStates(ConstructionalTransducer *t);
  virtual void CreateArcs(ConstructionalTransducer *t);
  virtual void CreatePhoneModel(ModelManager *models, int phone,
                                const PhoneContext &context);
  vector<AllophoneModel*> phone_models_;
  vector<State*> phone_states_;
  vector<int> units_;
};


// Initialize the transducer with one state per phone,
// and monophone models that may be shared among several
// phones.
class TiedModelTransducerInitialization : public BasicTransducerInitialization {
 public:
  virtual ~TiedModelTransducerInitialization() {}
  void SetPhoneMap(map<int, int> phone_map);

 protected:
  virtual void CreatePhoneModel(ModelManager *models, int phone,
                                const PhoneContext &context);
  map<int, int> phone_mapping_;
};


// Initialize in the same way as TiedModelTransducerInitialization
// but create only one state in the transducer for a set of mapped
// phone symbols.
class SharedStateTransducerInitialization : public
    TiedModelTransducerInitialization {
 public:
  virtual ~SharedStateTransducerInitialization() {}

 protected:
  virtual void CreateUnits(int num_phones);
  virtual void SetUnitHistory(int phone, PhoneContext *history) const;
  virtual void CreateStates(ConstructionalTransducer *t);
  map<int, list<int> > reverse_mapping_;
};


// Initialize in the same way as TiedModelTransducerInitialization
// but from final phones only allow transitions to initial phones
class WordBoundaryTransducerInitialization : public
    TiedModelTransducerInitialization {
 public:
  virtual ~WordBoundaryTransducerInitialization() {}
  void SetInitialPhones(const vector<int> &initial_phones);
  void SetFinalPhones(const vector<int> &final_phones);

 protected:
  void FillSet(const vector<int> &src, set<int> *dst) const;
  virtual void CreateArcs(ConstructionalTransducer *t);
  set<int> initial_phones_, final_phones_;
};


class TransducerInitializationFactory {
 public:
  static TransducerInitialization* Create(const string &name,
                                          const map<int, int> &phone_mapping,
                                          const vector<int> &initial_phones,
                                          const vector<int> &final_phones);

  static const char *kBasic;
  static const char *kTiedModel;
  static const char *kSharedState;
  static const char *kWordBoundary;
};


}  // namespace trainc

#endif  // TRANSDUCER_INIT_H_
