// phone_models.cc
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

#include <iterator>
#include <sstream>
#include <algorithm>
#include <string>
#include <utility>
#include "gaussian_model.h"
#include "phone_models.h"
#include "scorer.h"
#include "util.h"

using std::set;

namespace trainc {

HmmStateStat::HmmStateStat(int phone)
    : phone_(phone), num_obs_(-1), num_samples_(-1) {}

void HmmStateStat::AddStat(const Sample *sample) {
  samples_.push_back(sample);
  num_samples_ = -1;
  num_obs_ = -1;
}

void HmmStateStat::SetStats(const Samples::SampleList &sample_list) {
  std::transform(sample_list.begin(), sample_list.end(),
                 std::back_insert_iterator<SampleRefList>(samples_),
                 GetAddress<Sample>());
  num_samples_ = -1;
  num_obs_ = -1;
}

int HmmStateStat::NumContexts() {
  if (num_samples_ < 0) {
    num_samples_ = samples_.size();
  }
  return num_samples_;
}

int HmmStateStat::NumObservations() {
  if (num_obs_ < 0) {
    num_obs_ = 0;
    for (SampleRefList::const_iterator i = samples_.begin();
        i != samples_.end(); ++i)
      num_obs_ += (*i)->stat.weight();
  }
  return num_obs_;
}

void HmmStateStat::SumStatistics(Statistics *sum) const {
  SampleRefList::const_iterator dp;
  if (samples_.empty()) return;
  if (sum->dimension() <= 0)
    sum->Reset(samples_.front()->stat.dimension());
  for (dp = samples_.begin(); dp != samples_.end(); ++dp) {
    sum->Accumulate((*dp)->stat);
  }
}

// ======================================================

AllophoneModel* AllophoneModel::Clone() const {
  AllophoneModel *a = new AllophoneModel(NumStates());
  copy(states_.begin(), states_.end(), a->states_.begin());
  a->phones_.resize(phones_.size());
  copy(phones_.begin(), phones_.end(), a->phones_.begin());
  return a;
}

void AllophoneModel::SetStateModel(int state, AllophoneStateModel *m) {
  DCHECK(state < states_.size());
  DCHECK_EQ(m->state(), state);
  states_[state] = m;
}

void AllophoneModel::GetCommonContext(int position, ContextSet *context) const {
  DCHECK(states_[0] != NULL);
  *context = states_[0]->context(position);
  for (int state = 1; state < NumStates(); ++state)
    context->Intersect(states_[state]->context(position));
}

// New AllophoneModels are registered at their AllophoneStateModels.
AllophoneModel::SplitResult AllophoneModel::Split(
        int position,
        const AllophoneStateModel::SplitResult &new_state_models) const {
  SplitResult new_models(NULL, NULL);
  ContextSet common_context(states_[0]->context(position).Capacity());
  GetCommonContext(position, &common_context);
  for (int i = 0; i < 2; ++i) {
    AllophoneStateModel *state_model = GetPairElement(new_state_models, i);
    AllophoneModel* &model = GetPairElement(new_models, i);
    if (state_model) {
      ContextSet intersection = common_context;
      intersection.Intersect(state_model->context(position));
      if (intersection.IsEmpty()) {
        // new state model is not compatible with the other
        // state models of this phone model
        // -> don't create a new allophone model.
        continue;
      }
      model = Clone();
      model->SetStateModel(state_model->state(), state_model);
      // register allophone at all contained state models
      for (int s = 0; s < NumStates(); ++s) {
        model->GetStateModel(s)->AddAllophoneRef(model);
      }
    }
  }
  return new_models;
}

string AllophoneModel::ToString(bool full) const {
  std::stringstream ss;
  ss << "AllophoneModel [" << this << "]";
  ss << " phones=";
  copy(phones_.begin(), phones_.end(), std::ostream_iterator<int>(ss, ","));
  ss << " states=(";
  for (int s = 0; s < NumStates(); ++s) {
    if (states_[s]) {
      if (full)
        ss << states_[s]->ToString();
      else
        ss << states_[s] << " ";
    } else {
      ss << "NULL ";
    }
  }
  ss << ")";
  return ss.str();
}

// ======================================================

// Statistics of an AllophoneStateModel.
// Used to evaluate the gain of splitting HMM state models.
// Contains an HmmStateStat for every phone represented by the modeled unit.
class AllophoneStateModel::Data {
 public:
  Data()
      : num_observations_(0), num_seen_contexts_(0),
        have_cost_(false), cost_(0) {}
  ~Data() {
    STLDeleteElements(&data_);
  }
  // void AddDatum(const SuffStat &data, const GaussStats &counts);
  void AddStat(HmmStateStat *stat);
  void SplitData(int context_position, SplitResult *split) const;
  void EvalCost(const Scorer &scorer);
  void AddToModel(const string &distname, GaussianModel *model,
                  float variance_floor) const;
  bool HasCost() const { return have_cost_; }
  float cost() const { return cost_; }
  int num_observations() const { return num_observations_; }
  int num_seen_contexts() const { return num_seen_contexts_; }
 private:
  void SumCounts(Statistics *sum) const;
  void SplitContext(int context_position,
                    const Partition &partition,
                    SplitResult *split) const;
  void SplitCenter(const Partition &partition,
                    SplitResult *split) const;

  vector<HmmStateStat*> data_;
  int num_observations_;
  int num_seen_contexts_;
  bool have_cost_;
  float cost_;

  DISALLOW_COPY_AND_ASSIGN(Data);
};

void AllophoneStateModel::Data::AddStat(HmmStateStat *stat) {
  data_.push_back(stat);
  num_seen_contexts_ += stat->NumContexts();
  num_observations_ += stat->NumObservations();
}

void AllophoneStateModel::Data::SplitData(
    int context_position, SplitResult *split) const {
  DCHECK(split->first && split->second);
  Partition partition(
      split->first->context(context_position),
      split->second->context(context_position));
  for (int c = 0; c < 2; ++c) {
    AllophoneStateModel *state_model = GetPairElement(*split, c);
    DCHECK(state_model->data_ == NULL);
    state_model->data_ = new Data();
  }
  if (context_position == 0)
    SplitCenter(partition, split);
  else
    SplitContext(context_position, partition, split);
}

// Distribute the HmmStateStats to the new AllophoneStateModels
// depending on their center phone set.
void AllophoneStateModel::Data::SplitCenter(
  const Partition &partition,
  SplitResult *split) const {
  vector<HmmStateStat*>::const_iterator sp;
  for (sp = data_.begin(); sp != data_.end(); ++sp) {
    const HmmStateStat &stat = *(*sp);
    Data *to_add = NULL;
    int phone = stat.phone();
    for (int c = 0; c < 2; ++c) {
      if (GetPairElement(partition, c).HasElement(phone)) {
        DCHECK(to_add == NULL);
        to_add = GetPairElement(*split, c)->data_;
      }
    }
    DCHECK(to_add != NULL);
    to_add->AddStat(new HmmStateStat(stat));
  }
}

// Distribute the SuffStats of all HmmStateStats to the new AllphoneStateModels.
void AllophoneStateModel::Data::SplitContext(
    int context_position,
    const Partition &partition,
    SplitResult *split) const {
  vector<HmmStateStat*>::const_iterator sp;
  for (sp = data_.begin(); sp != data_.end(); ++sp) {
    const HmmStateStat &stat = *(*sp);
    pair<HmmStateStat*, HmmStateStat*> new_stats(NULL, NULL);
    for (int c = 0; c < 2; ++c) {
      HmmStateStat* &s = GetPairElement(new_stats, c);
      s = new HmmStateStat(stat.phone());
    }
    HmmStateStat::SampleRefList::const_iterator dp;
    for (dp = stat.stats().begin(); dp != stat.stats().end(); ++dp) {
      int phone = -1;
      if (context_position > 0)
        phone = (*dp)->right_context_[context_position - 1];
      else if (context_position < 0)
        phone = (*dp)->left_context_[-context_position - 1];
      else
        CHECK(false);
      // apply phone symbol index shift
      DCHECK_GT(phone, 0);
      --phone;
      for (int c = 0; c < 2; ++c) {
        if (GetPairElement(partition, c).HasElement(phone)) {
          GetPairElement(new_stats, c)->AddStat(*dp);
        }
      }
    }
    for (int c = 0; c < 2; ++c)
      GetPairElement(*split, c)->data_->AddStat(GetPairElement(new_stats, c));
  }
}



void AllophoneStateModel::Data::SumCounts(Statistics *sum) const {
  for (vector<HmmStateStat*>::const_iterator s = data_.begin();
      s != data_.end(); ++s)
    (*s)->SumStatistics(sum);
}

// Evaluates the cost of the AllophoneStateModel by estimating the ML
// distribution and determining the data likelihood under that distribution.
void AllophoneStateModel::Data::EvalCost(const Scorer &scorer) {
  Statistics sum;
  SumCounts(&sum);
  cost_ = scorer.score(sum);
  have_cost_ = true;
}

// Add the summed sufficient statistics for the AllophoneStateModel
// and adds them as new model to the given GaussianModel.
void AllophoneStateModel::Data::AddToModel(
    const string &distname, GaussianModel *model,
    float variance_floor) const {
  Statistics sum;
  SumCounts(&sum);
  model->Estimate(distname, sum, variance_floor);
}

// ======================================================

AllophoneStateModel::~AllophoneStateModel() {
  delete data_;
}

void AllophoneStateModel::AddAllophoneRef(AllophoneModel *model) {
  allophones_.push_front(model);
}

void AllophoneStateModel::RemoveAllophoneRef(AllophoneModel *model) {
  allophones_.remove(model);
}

AllophoneStateModel::SplitResult AllophoneStateModel::Split(
    int position, const ContextQuestion &question) const {
  AllophoneStateModel* new_models[2] = {Clone(), Clone()};
  for (int i = 0; i < 2; ++i) {
    new_models[i]->context_.GetContextRef(position)->Intersect(
        question.GetPhoneSet(i));
    if (new_models[i]->context_.GetContext(position).IsEmpty()) {
      delete new_models[i];
      new_models[i] = NULL;
    }
  }
  return std::make_pair(new_models[0], new_models[1]);
}

void AllophoneStateModel::SplitAllophones(
    int position, const SplitResult &new_models, ModelSplit *split) const {
  typedef AllophoneStateModel::AllophoneRefList::const_iterator ModelIter;
  for (ModelIter it = allophones_.begin(); it != allophones_.end(); ++it) {
    AllophoneModel *old_model = *it;
    AllophoneModel::SplitResult new_phones =
        old_model->Split(position, new_models);
    split->phone_models.push_back(AllophoneModelSplit(old_model, new_phones));
  }
  DCHECK_EQ(allophones_.size(), split->phone_models.size());
}

void AllophoneStateModel::AddStatistics(HmmStateStat *stat) {
  if (!data_) data_ = new Data();
  data_->AddStat(stat);
}

void AllophoneStateModel::SplitData(int position, SplitResult *split) const {
  data_->SplitData(position, split);
}

// This will set the cost_ member of the data_ member of both
// AllophoneStateModels in split.
void AllophoneStateModel::ComputeCosts(
    SplitResult *split, const Scorer &scorer) const {
  if (!data_->HasCost())
    data_->EvalCost(scorer);
  split->first->data_->EvalCost(scorer);
  split->second->data_->EvalCost(scorer);
}

void AllophoneStateModel::AddToModel(
    const string &distname, GaussianModel *model,
    float variance_floor) const {
  data_->AddToModel(distname, model, variance_floor);
}

float AllophoneStateModel::GetGain(const SplitResult &split) const {
  return data_->cost() -
      (split.first->data_->cost() + split.second->data_->cost());
}

int AllophoneStateModel::NumObservations() const {
  DCHECK(data_ != NULL);
  return data_->num_observations();
}

int AllophoneStateModel::NumSeenContexts() const {
  DCHECK(data_ != NULL);
  return data_->num_seen_contexts();
}

float AllophoneStateModel::GetCost() const {
  CHECK(data_ != NULL);
  return data_->cost();
}

string AllophoneStateModel::ToString() const {
  std::stringstream ss;
  ss << "AllophoneStateModel [" << this << "] ";
  ss << "state=" << state_;
  ss << " context=" << context_.ToString();
  ss << " allophones=(";
  for (AllophoneRefList::const_iterator i = allophones_.begin();
      i != allophones_.end(); ++i) {
    ss << "{" << (*i)->ToString() << "} ";
  }
  ss << ")";
  return ss.str();
}

// ======================================================

ModelManager::~ModelManager() {
  // collect all AllophoneModel objects and
  // delete AllophoneStateModel objects
  set<AllophoneModel*> models;
  for (StateModelList::const_iterator s = state_models_.begin();
      s != state_models_.end(); ++s) {
    AllophoneStateModel::AllophoneRefList::const_iterator a =
        (*s)->GetAllophones().begin();
    for (; a != (*s)->GetAllophones().end(); ++a) {
      models.insert(*a);
    }
    delete *s;
  }
  // delete AllophoneModel objects
  for (set<AllophoneModel*>::const_iterator i = models.begin();
      i != models.end(); ++i) {
    delete *i;
  }
}

AllophoneModel* ModelManager::InitAllophoneModel(
    int phone, int num_states, const PhoneContext &context) {
  CHECK(context.GetContext(0).HasElement(phone));
  AllophoneModel *model = new AllophoneModel(phone, num_states);
  PhoneContext state_context = context;
  for (int s = 0; s < model->NumStates(); ++s) {
    AllophoneStateModel *state_model = new AllophoneStateModel(s,
                                                               state_context);
    model->SetStateModel(s, state_model);
    state_model->AddAllophoneRef(model);
    AddStateModel(state_model);
  }
  return model;
}

// Remove the allophone model from all of its state models.
// The allophone model is not deleted yet.
void ModelManager::RemoveAllophoneModel(AllophoneModel *model) const {
  for (int s = 0; s < model->NumStates(); ++s) {
    model->GetStateModel(s)->RemoveAllophoneRef(model);
  }
}

ModelManager::StateModelRef ModelManager::AddStateModel(
    AllophoneStateModel *state_model) {
  ++num_state_models_;
  state_models_.push_front(state_model);
  return state_models_.begin();
}

ModelManager::StateModelRef ModelManager::RemoveStateModel(
    StateModelRef state_model) {
  DCHECK((*state_model)->GetAllophones().empty());
  --num_state_models_;
  delete *state_model;
  return state_models_.erase(state_model);
}

ModelManager::StateModelRef ModelManager::ApplySplit(
    int position, StateModelRef old_state_model,
    AllophoneStateModel::SplitResult *new_models, ModelSplit *split_result) {
  (*old_state_model)->SplitAllophones(position, *new_models, split_result);
  const AllophoneStateModel::AllophoneRefList &old_allophones =
      (*old_state_model)->GetAllophones();
  while (!old_allophones.empty())
    RemoveAllophoneModel(old_allophones.front());
  for (int c = 0; c < 2; ++c) {
    AllophoneStateModel* new_state_model = GetPairElement(*new_models, c);
    StateModelRef &new_ref = GetPairElement(split_result->state_models, c);
    if (new_state_model)
      new_ref = AddStateModel(new_state_model);
  }
  return RemoveStateModel(old_state_model);
}

void ModelManager::DeleteOldModels(
    vector<AllophoneModelSplit> *phone_models) const {
  for (vector<AllophoneModelSplit>::iterator i = phone_models->begin();
      i != phone_models->end(); ++i) {
    delete i->old_model;
    i->old_model = NULL;
  }
}

}  // namespace trainc

