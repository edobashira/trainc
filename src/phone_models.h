// phone_models.h
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
// Models of context dependent phones and their HMM states.
// A context dependent HMM is described by an AllophoneModel, which consists
// of a phone and a sequence of states.
// An HMM state is defined by AllophoneStateModel.
// An AllophoneStateModel can appear in several AllophoneModels having the
// same phone, due to the parameter tying of HMM state emission models, but
// only at the same state position (no parameter sharing between states).
// An AllophoneStateModel is valid in one or more phone contexts, which are
// defined by PhoneContext.
// When a state model is split, i.e. the set of its valid phone contexts is
// split, a pair of new AllophoneModels has to be created for each
// AllophoneModel that includes the AllophoneStateModel. Such a split is
// described with the struct ModelSplit.
// The set of AllophoneStateModels is governed by the ModelManager.

#ifndef PHONE_MODELS_H_
#define PHONE_MODELS_H_

#include <list>
#include <set>
#include <ext/slist>
#include <string>
#include <utility>
#include <vector>
#include "debug.h"
#include "context_set.h"
#include "sample.h"

using std::list;
using __gnu_cxx::slist;
using std::pair;

namespace trainc {

class Scorer;
class GaussianModel;

// Statistics for a context dependent HMM state.
class HmmStateStat {
 public:
  typedef std::vector<const Sample*> SampleRefList;

  // Initialize statistics for HMM state of the given phone.
  explicit HmmStateStat(int phone);

  // Set the phone represented by the HMM.
  void SetPhone(int phone) { phone_ = phone; }

  // Phone represented by the HMM.
  int phone() const { return phone_; }

  // Set the samples for the model.
  void SetStats(const Samples::SampleList &samples);

  // Add a Sample object for the model.
  void AddStat(const Sample *sample);

  // Reference to the list of SuffStat objects.
  const SampleRefList& stats() const { return samples_; }

  // Number of seen contexts
  int NumContexts();

  // Total number of observations
  int NumObservations();

  // Sum the contained statistics
  void SumStatistics(Statistics *sum) const;

  // Dimensionality of the used features
  void Dimension() const;

 private:
  int phone_;
  int num_obs_, num_samples_;
  SampleRefList samples_;
};


class AllophoneModel;
struct ModelSplit;

// Model of a HMM state of a context dependent phone.
// The object keeps track of the AllophoneModels that contain it.
class AllophoneStateModel {
 public:
  typedef slist<AllophoneModel*> AllophoneRefList;
  typedef pair<AllophoneStateModel*, AllophoneStateModel*> SplitResult;

  // Setup the model for the given state of a context dependent HMM.
  AllophoneStateModel(int state, const PhoneContext &context)
      : data_(NULL), state_(state), context_(context) {}

  ~AllophoneStateModel();

  // Create a copy of the model.
  // The new object has an empty list of referring allophone models.
  // Remark: the Data object is not copied.
  AllophoneStateModel* Clone() const {
    return new AllophoneStateModel(state_, context_);
  }

  // Register an AllophoneModel object that contains this AllophoneStateModel.
  void AddAllophoneRef(AllophoneModel *model);

  // Unregister an AllophoneModel object.
  void RemoveAllophoneRef(AllophoneModel *model);

  // List of AllophoneModel objects that contain this AllophoneStateModel.
  const AllophoneRefList& GetAllophones() const { return allophones_; }

  // Split the model by partitioning the phones at the given context
  // position into two disjunct sets.
  // The underlying statistics are not split and the new models do not
  // have valid statistics. The statistics have to be updated using
  // SplitData().
  SplitResult Split(int position, const ContextQuestion &question) const;

  // Create new AllophoneModels for the AllophoneStateModels new_models
  // and store them in split->phone_models.
  void SplitAllophones(int position, const SplitResult &new_models,
                       ModelSplit *split) const;

  // Add statistics to the model.
  void AddStatistics(HmmStateStat *stat);

  // Distribute the statistics to the two new models in split.
  // position is the context position used to split the model.
  void SplitData(int position, SplitResult *split) const;

  // Compute the cost of both new AllophoneStateModels in split.
  void ComputeCosts(SplitResult *split, const Scorer &scorer) const;

  // Add the statistics associated with this model to the given GaussianModel
  // using the given name.
  // Finalize() has to be called beforehand to ensure that the underlying
  // model has been created.
  void AddToModel(const string &distname,
                  GaussianModel *model,
                  float variance_floor) const;

  // Computes the gain in likelihood achieved by splitting this
  // AllophoneStateModel into the two given AllophoneStateModels.
  // Requires that ComputeCosts has been executed before on the given
  // SplitResult.
  float GetGain(const SplitResult &split) const;

  // Number of seen contexts.
  int NumSeenContexts() const;

  // Number of seen observations.
  int NumObservations() const;

  float GetCost() const;

  // HMM state
  int state() const { return state_; }

  // Set of phones at the context position.
  const ContextSet& context(int position) const {
    return context_.GetContext(position);
  }

  const PhoneContext& GetContext() const {
    return context_;
  }

  PhoneContext* GetContextRef() {
    return &context_;
  }

  // convert to string representation.
  // for debugging purposes.
  string ToString() const;

 private:
  class Data;
  Data *data_;
  int state_;
  AllophoneRefList allophones_;
  PhoneContext context_;

  DISALLOW_COPY_AND_ASSIGN(AllophoneStateModel);
};


// Model of a context dependent unit.
// A unit may represent several phones.
// The model consists of an AllophoneStateModel for each HMM state,
// which may be shared among several AllophoneModels.
class AllophoneModel {
 public:
  typedef pair<AllophoneModel*, AllophoneModel*> SplitResult;

  // Setup a model with num_state states and center phone phone.
  AllophoneModel(int phone, int num_states)
      : states_(static_cast<size_t>(num_states), NULL),
        phones_(1, phone) {}

  // Create a copy of the model.
  // The AllophoneStateModels remain the same.
  AllophoneModel* Clone() const;

  // Add a phone to the set of phones represented by the modeled unit.
  void AddPhone(int phone) { phones_.push_back(phone); }

  // Reset the set of phones.
  void ClearPhones() { phones_.clear(); }

  // The center phones of the CD phone.
  const vector<int>& phones() const { return phones_; }

  // TODO(rybach): remove after transition!
  // int phone() const { return phones_.front(); }

  // Number of HMM states.
  int NumStates() const { return states_.size(); }

  // Model of a specific HMM state.
  AllophoneStateModel* GetStateModel(int state) const {
    DCHECK(state < states_.size());
    return states_[state];
  }

  // Set the model for a HMM state.
  // Ownership of the AllophoneStateModel remains at the caller.
  void SetStateModel(int state, AllophoneStateModel *m);

  // Compute the intersection of the context sets at the given position
  // of all AllophoneStateModels.
  void GetCommonContext(int position, ContextSet *context) const;

  // Create two new AllophoneModels with the same sequence of
  // AllophoneStateModels except for the HMM state modeled by the two
  // new AllophoneStateModels in new_state models.
  // Position is the context position used to split the state models.
  SplitResult Split(int position,
                    const AllophoneStateModel::SplitResult
                    &new_state_models) const;

  // convert to to string representation.
  // for debugging purposes.
  string ToString(bool full = false) const;

 private:
  // Only used by Clone()
  explicit AllophoneModel(int num_states)
      : states_(static_cast<size_t>(num_states), NULL) {}

  vector<AllophoneStateModel*> states_;
  vector<int> phones_;

  DISALLOW_COPY_AND_ASSIGN(AllophoneModel);
};


struct AllophoneModelSplit;

// Manages the AllophoneModel and AllophoneStateModel objects.
// The ModelManager will delete all remaining objects, that have
// been registered, during its destruction.
class ModelManager {
 public:
  typedef list<AllophoneStateModel*> StateModelList;
  typedef StateModelList::iterator StateModelRef;
  typedef StateModelList::const_iterator StateModelConstRef;
  typedef pair<StateModelRef, StateModelRef> SplitResult;

  ModelManager() : num_state_models_(0) {}

  // Delete all remaining AllophoneStateModel objects and all
  // AllophoneModel objects associated with them.
  ~ModelManager();

  // Create a new AllophoneModel for the given phone with num_states
  // HMM states and AllophoneStateModels with the given context.
  AllophoneModel* InitAllophoneModel(int phone, int num_states,
                                     const PhoneContext &context);

  // Number of AllophoneStateModels currently registered.
  int NumStateModels() const { return num_state_models_; }

  // Add a new AllophoneStateModel.
  // Returns an iterator that can be used to remove the AllophoneStateModel.
  StateModelRef AddStateModel(AllophoneStateModel *state_model);

  // Remove the AllophoneStateModel state_model.
  // The iterator will be invalid after this call.
  // Returns the next valid iterator or GetStateModels().end().
  StateModelRef RemoveStateModel(StateModelRef state_model);

  // Mutable access to the list of all AllophoneStateModels
  StateModelList* GetStateModelsRef() {
    return &state_models_;
  }

  // Access to the list of all AllophoneStateModels
  const StateModelList& GetStateModels() const {
    return state_models_;
  }

  // Applies the split by creating new models and deleting the old models.
  // New AllophoneStateModels are created using AllophoneModel::Split
  // and stored in split_result->phone_models.
  // The AllophoneStateModel old_state_model and all AllophoneModels it
  // occurs in will be deleted.
  // old_state_model will be invalid after this call.
  // The iterator returned will point to the next valid
  // AllophoneStateModel in state_models_ or to state_models_.end().
  // Iterators to the added AllophoneStateModels are stored in
  // split_result->state_models.
  StateModelRef ApplySplit(int position, StateModelRef old_state_model,
                           AllophoneStateModel::SplitResult *new_models,
                           ModelSplit *split_result);

  // Deletes the AllophoneModels in phone_models[].old_model.
  // Must be called after ApplySplit().
  void DeleteOldModels(vector<AllophoneModelSplit> *phone_models) const;


 private:
  void RemoveAllophoneModel(AllophoneModel *model) const;

  // maintains the number of registered state models,
  int num_state_models_;
  StateModelList state_models_;

  DISALLOW_COPY_AND_ASSIGN(ModelManager);
};


// A tuple of an AllophoneModel and the AllophoneModels it has been split into.
struct AllophoneModelSplit {
  AllophoneModel *old_model;
  AllophoneModel::SplitResult new_models;
  AllophoneModelSplit(AllophoneModel *old_m, AllophoneModel::SplitResult new_m)
      : old_model(old_m), new_models(new_m) {}
};

// Result of splitting an AllophoneStateModel and all the
// AllophoneModels it occurs in.
// Result of ModelManager::ApplySplit.
struct ModelSplit {
  // new AllophoneStateModels (list iterator)
  ModelManager::SplitResult state_models;
  // involved old and new AllophoneModels
  vector<AllophoneModelSplit> phone_models;
};


// Collects information about all phones.
// Store total number of phones, number of HMM states per phone and
// tags phones as context independent / dependent.
// All phone indexes are zero based (symbol table key - 1).
class Phones {
 public:
  explicit Phones(int num_phones)
      : num_phones_(num_phones),
        num_hmm_states_(num_phones, -1),
        is_ci_phone_(num_phones, false) {}

  // Set the number of HMM states of a given phone.
  void SetPhoneLength(int phone, int hmm_states) {
    DCHECK_LT(phone, num_hmm_states_.size());
    num_hmm_states_[phone] = hmm_states;
  }

  // Set the number of HMM states of all phones.
  void SetPhoneLenghts(const std::vector<int> &phone_lenghts) {
    CHECK_EQ(phone_lenghts.size(), num_phones_);
    num_hmm_states_ = phone_lenghts;
  }

  // Mark the phone as context independently modeled
  void SetCiPhone(int phone) {
    DCHECK_LT(phone, is_ci_phone_.size());
    is_ci_phone_[phone] = true;
  }

  // Total number phones
  int NumPhones() const {
    return num_phones_;
  }

  // The number of HMM states of the given phone.
  // Returns -1 if the number of HMM states has not been set.
  int NumHmmStates(int phone) const {
    DCHECK_LT(phone, num_hmm_states_.size());
    // DCHECK_GE(num_hmm_states_[phone], 0);
    return num_hmm_states_[phone];
  }

  // Is the given phone modeled context independent.
  bool IsCiPhone(int phone) const {
    DCHECK_GE(phone, 0);
    DCHECK_LT(phone, is_ci_phone_.size());
    return is_ci_phone_[phone];
  }

 private:
  int num_phones_;
  vector<int> num_hmm_states_;
  vector<bool> is_ci_phone_;
};


}  // namespace trainc

#endif  // PHONE_MODELS_H_
