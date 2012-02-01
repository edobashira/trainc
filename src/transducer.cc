// transducer.cc
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

#include <algorithm>


#include "transducer.h"
#include "state_splitter.h"
#include "split_predictor.h"
#include "phone_models.h"
#include "util.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_THREADS
#include "thread.h"
using threads::Mutex;
using threads::MutexLock;
#endif


namespace trainc {

#ifndef HAVE_THREADS
struct Mutex {};
struct MutexLock { MutexLock(Mutex*) {} };
#endif  // HAVE_THREADS


// set of predecessor states.
// thread-safe if compiled with thread support.
// thread-safety is necessary because the cache has to be mutable
// and several threads may access the same state in parallel.
class State::PredecessorCache : StateRefSet {
public:
  PredecessorCache() : cached_(false) {}
  bool Cached() const {
    MutexLock l(&mutex_);
    return cached_;
  }
  void Reset() {
    MutexLock l(&mutex_);
    cached_ = false;
  }
  const StateRefSet& Get(const State &state) {
    MutexLock l(&mutex_);
    if (!cached_) InternalUpdate(state);
    return *this;
  }
  void Update(const State &state) {
    MutexLock l(&mutex_);
    InternalUpdate(state);
  }
 private:
  void InternalUpdate(const State &state) {
    clear();
    for (ArcRefList::const_iterator arc = state.incoming_arcs_.begin();
        arc != state.incoming_arcs_.end(); ++arc)
      insert((*arc)->source());
    cached_ = true;
  }
  bool cached_;
  mutable Mutex mutex_;
};

State::State(const PhoneContext &history)
    : history_(history), predecessors_(new PredecessorCache()) {}

State::~State() {
  delete predecessors_;
}
State::ArcRef State::AddArc(
    const AllophoneModel *input, int output, State *target) {
  arcs_.push_front(Arc(this, target, input, output));
  return arcs_.begin();
}

const State::StateRefSet& State::GetPredecessorStates() const {
  return predecessors_->Get(*this);
}

void State::AddIncomingArc(ArcRef arc) {
  incoming_arcs_.insert(arc);
  predecessors_->Reset();
}

void State::RemoveIncomingArc(ArcRef arc) {
  incoming_arcs_.erase(arc);
  predecessors_->Reset();
}

void State::RemoveArc(ArcRef arc) {
  arcs_.erase(arc);
}

void State::ClearArcs() {
  arcs_.clear();
}

// ==========================================================

ConstructionalTransducer::ConstructionalTransducer(
    int num_phones, int num_left_contexts, int num_right_contexts,
    bool center_set)
    : num_phones_(num_phones),
      num_left_contexts_(num_left_contexts),
      num_right_contexts_(num_right_contexts),
      center_set_(center_set),
      state_map_(num_phones * num_phones),
      num_states_(0),
      splitter_(new StateSplitter(this, num_left_contexts, num_right_contexts,
                                  num_phones, center_set)),
      observer_(NULL) {}

ConstructionalTransducer::~ConstructionalTransducer() {
  // delete all State objects
  STLDeleteContainerPairSecondPointers(state_map_.begin(), state_map_.end());
  delete splitter_;
}

ConstructionalTransducer* ConstructionalTransducer::Clone() const {
  return new ConstructionalTransducer(num_phones_, num_left_contexts_,
      num_right_contexts_, center_set_);
}

int ConstructionalTransducer::NumStates() const {
  return num_states_;
}

State* ConstructionalTransducer::GetState(const PhoneContext &history) const {
  StateHashMap::const_iterator s = state_map_.find(history);
  if (s == state_map_.end()) {
    return NULL;
  } else {
    return s->second;
  }
}

State* ConstructionalTransducer::AddState(const PhoneContext &history) {
  DCHECK_EQ(history.NumRightContexts(), 0);
  State *s = new State(history);
  pair<StateHashMap::iterator, bool> r =
      state_map_.insert(std::make_pair(history, s));
  CHECK(r.second);  // phone context does not exist
  ++num_states_;
  VLOG(2) << "CT::AddState " << s;
  if (observer_) observer_->NotifyAddState(s);
  return s;
}

void ConstructionalTransducer::RemoveState(const State *state) {
  VLOG(2) << "CT::RemoveState " << state;
  DCHECK(state->GetArcs().empty());
  state_map_.erase(state->history());
  if (observer_) observer_->NotifyRemoveState(state);
  delete state;
  --num_states_;
}

// Update arcs_with_model_
void ConstructionalTransducer::SetModelToArc(
    State::ArcRef arc, const AllophoneModel *model) {
  ModelToArcMap::iterator it = arcs_with_model_.find(model);
  if (it == arcs_with_model_.end()) {
    pair<ModelToArcMap::iterator, bool> r = arcs_with_model_.insert(
        ModelToArcMap::value_type(model, ModelToArcMap::data_type()));
    it = r.first;
  }
  it->second.insert(arc);
}

// Update arcs_with_model_
void ConstructionalTransducer::RemoveModelToArc(
    State::ArcRef arc, const AllophoneModel *model) {
  ModelToArcMap::iterator it = arcs_with_model_.find(model);
  DCHECK(it != arcs_with_model_.end());
  it->second.erase(arc);
}

State::ArcRef ConstructionalTransducer::AddArc(
    State *source, State *target, const AllophoneModel *input, int output) {
  // VLOG(2) << "CT::AddArc: from=" << source << " to=" << target << " " << input << " " << output;
  State::ArcRef arc = source->AddArc(input, output, target);
  target->AddIncomingArc(arc);
  SetModelToArc(arc, input);
  if (observer_) observer_->NotifyAddArc(arc);
  return arc;
}

void ConstructionalTransducer::UpdateArcInput(
    State::ArcRef arc, const AllophoneModel *new_input) {
  RemoveModelToArc(arc, arc->input());
  SetModelToArc(arc, new_input);
  arc->SetInput(new_input);
}

void ConstructionalTransducer::RemoveArc(State::ArcRef arc) {
  // VLOG(2) << "CT::RemoveArc: from=" << arc->source() << " to=" << arc->target() << " " << arc->input() << " " << arc->output();
  arc->target()->RemoveIncomingArc(arc);
  RemoveModelToArc(arc, arc->input());
  State *source = arc->source();
  if (observer_) observer_->NotifyRemoveArc(arc);
  source->RemoveArc(arc);
}

void ConstructionalTransducer::RemoveModel(const AllophoneModel *m) {
  ModelToArcMap::iterator element = arcs_with_model_.find(m);
  if (element == arcs_with_model_.end()) {
    // TODO(rybach): why can that happen?
    return;
  }
  CHECK(element != arcs_with_model_.end());
  CHECK(element->second.empty());
  arcs_with_model_.erase(element);
}

// Collect the set of states involved in splitting the given models and
// calculate the number of new states introduced by this split.
/*
int ConstructionalTransducer::GetNumberOfNewStates(
    int context_pos, const ContextQuestion &question,
    const AllophoneStateModel::AllophoneRefList &models,
    StateUpdates *updates) const {
  typedef AllophoneStateModel::AllophoneRefList::const_iterator ModelIter;
  if (context_pos == 1)
    return 0;
  DCHECK_GT(models.size(), 0);
  StateRefSet states;
  for (ModelIter model = models.begin(); model != models.end(); ++model) {
    GetStatesForModel(*model, &states);
  }
  return predictor_->Count(context_pos, question, states, updates);
}
*/

void ConstructionalTransducer::ApplyModelSplit(
    int context_pos, const ContextQuestion *question, AllophoneModel *old_model,
    int hmm_state, const AllophoneModel::SplitResult &new_models) {
  if (context_pos <= 0) {
    // split left context or center
    splitter_->SplitHistory(context_pos, old_model, hmm_state, new_models);
  } else {
    // split right context
    splitter_->SplitFuture(context_pos, old_model, hmm_state, new_models);
  }
  RemoveModel(old_model);
}


// Collect the arcs which have the AllophoneStateModel model as input.
void ConstructionalTransducer::GetArcsWithModel(
    const AllophoneModel *model, vector<State::ArcRef> *arcs) const {
  arcs->clear();
  ModelToArcMap::const_iterator arc_list = arcs_with_model_.find(model);
  if (arc_list == arcs_with_model_.end()) return;
  std::copy(arc_list->second.begin(), arc_list->second.end(),
       std::back_insert_iterator< vector<State::ArcRef> >(*arcs));
}

// Collect the set of states which have an outgoing arc with model as input.
void ConstructionalTransducer::GetStatesForModel(
    const AllophoneModel *model, StateRefSet *states) const {
  // don't use GetArcsWithModel so save the allocation of an
  // intermediate vector of Arcs
  ModelToArcMap::const_iterator arc_list = arcs_with_model_.find(model);
  if (arc_list == arcs_with_model_.end()) return;
  for (ModelToArcMap::data_type::const_iterator arc = arc_list->second.begin();
      arc != arc_list->second.end(); ++arc)
    states->insert((*arc)->source());
}

// Find the set of predecessor states of the given set
// of states. The predecessors are added to given set.
void ConstructionalTransducer::GetPredecessorStatesOfSet(
    const State::StateRefSet &states, StateRefSet *predecessors) const {
  for (StateRefSet::const_iterator s = states.begin();
       s != states.end(); ++s) {
    const StateRefSet &p = (*s)->GetPredecessorStates();
    predecessors->insert(p.begin(), p.end());
  }
}

AbstractSplitPredictor* ConstructionalTransducer::CreateSplitPredictor() const {
  return new SplitPredictor(*this);
}

}  // namespace trainc
