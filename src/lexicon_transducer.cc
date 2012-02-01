// lexicon_transducer.cc
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
// Copyright 2011 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)

#include <algorithm>
#include <iterator>
#include "epsilon_closure.h"
#include "lexicon_init.h"
#include "lexicon_split_predictor.h"
#include "lexicon_state_splitter.h"
#include "lexicon_transducer.h"
#include "shifted_init.h"
#include "shifted_split_predictor.h"
#include "shifted_state_splitter.h"
#include "state_siblings.h"
#include "util.h"

namespace trainc {

size_t LexiconState::NumOutputEpsilons() const {
  size_t n = 0;
  for (ArcList::const_iterator a = arcs_.begin(); a != arcs_.end(); ++a)
    if (!a->olabel)
      ++n;
  return n;
}

void LexiconState::UpdateContext() {
  ContextSet *left_context = context_.GetContextRef(0);
  ContextSet *right_context = context_.GetContextRef(1);
  left_context->Clear();
  right_context->Clear();
  for (ForwardArcIterator aiter(this); !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (arc.model)
      right_context->Add(arc.ilabel);
  }
  for (BackwardArcIterator aiter(this); !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (arc.model)
      left_context->Add(arc.ilabel);
  }
}

LexiconTransducerImpl::LexiconTransducerImpl() {
  SetType("lexicon");
  SetProperties(fst::kNotAcceptor | fst::kNotILabelSorted |
                fst::kNotOLabelSorted | fst::kExpanded);
}

LexiconTransducer::StateId LexiconTransducerImpl::AddState(
    const PhoneContext &c) {
  StateId s;
  if (free_states_.empty()) {
    s = states_.size();
    states_.push_back(new LexiconState(c));
  } else {
    s = free_states_.back();
    free_states_.pop_back();
    states_[s] = new LexiconState(c);
  }
  return s;
}

void LexiconTransducerImpl::RemoveState(StateId s) {
  State *state = states_[s];
  DCHECK(state);
  State::ArcList &arcs = state->Arcs();
  for (ArcRef a = arcs.begin(); a != arcs.end();)
    a = RemoveArc(s, a);
  state->Clear();
  delete state;
  states_[s] = NULL;
  deleted_states_.push_back(s);
  if (IsStart(s))
    start_.erase(s);
}

void LexiconTransducerImpl::SetModelToArc(ArcRef arc,
                                          const AllophoneModel *model) {
  ModelToArcMap::iterator it = arcs_with_model_.find(model);
  if (it == arcs_with_model_.end()) {
    pair<ModelToArcMap::iterator, bool> r = arcs_with_model_.insert(
        ModelToArcMap::value_type(model, ModelToArcMap::data_type()));
    it = r.first;
  }
  it->second.insert(arc);
}

void LexiconTransducerImpl::RemoveModelToArc(
    State::ArcRef arc, const AllophoneModel *model) {
  ModelToArcMap::iterator it = arcs_with_model_.find(model);
  DCHECK(it != arcs_with_model_.end());
  it->second.erase(arc);
}

LexiconTransducer::ArcRef LexiconTransducerImpl::AddArc(StateId s,
                                                        const Arc &arc) {
  DCHECK(states_[s]);
  State::ArcRef arc_ref = states_[s]->AddArc(arc);
  arc_ref->prevstate = s;
  SetModelToArc(arc_ref, arc.model);
  DCHECK(states_[arc.nextstate]);
  states_[arc.nextstate]->AddIncoming(arc_ref);
  return arc_ref;
}

LexiconTransducer::ArcRef LexiconTransducerImpl::RemoveArc(StateId s,
                                                           ArcRef arc) {
  RemoveModelToArc(arc, arc->model);
  DCHECK_LT(arc->nextstate, states_.size());
  DCHECK(states_[arc->nextstate]);
  states_[arc->nextstate]->RemoveIncoming(arc);
  DCHECK_LT(s, states_.size());
  DCHECK(states_[s]);
  return states_[s]->RemoveArc(arc);
}

void LexiconTransducerImpl::UpdateArc(State::ArcRef arc,
                                      const AllophoneModel *new_model) {
  DCHECK(states_[arc->prevstate]);
  RemoveModelToArc(arc, arc->model);
  SetModelToArc(arc, new_model);
  arc->model = new_model;
}

void LexiconTransducerImpl::PurgeStates() {
  if (free_states_.empty()) {
    free_states_.swap(deleted_states_);
  } else {
    std::copy(deleted_states_.begin(), deleted_states_.end(),
              std::back_inserter(free_states_));
    deleted_states_.clear();
  }
}

void LexiconTransducerImpl::InitStateIterator(
    fst::StateIteratorData<LexiconArc> *data) const {
  data->base = 0;
  data->nstates = states_.size();
}

void LexiconTransducerImpl::InitArcIterator(
    StateId s, fst::ArcIteratorData<Arc> *data) const {
  if (states_[s]) {
    data->base = new LexiconArcIterator(*states_[s]);
    data->ref_count = 0;
  } else {
    data->base = 0;
    data->narcs = 0;
    data->ref_count = 0;
    data->arcs = 0;
  }
}

// ================================================================

LexiconTransducer::LexiconTransducer(const LexiconTransducer &o)
    : fst::ImplToExpandedFst<LexiconTransducerImpl>(o),
        num_phones_(o.num_phones_), det_split_(o.det_split_),
        shifted_(o.shifted_), empty_context_(o.empty_context_), c_(o.c_),
        splitter_(NULL), empty_model_(o.empty_model_->Clone()) {
  siblings_ = o.siblings_ ? new LexiconStateSiblings(*o.siblings_) : NULL;
  for (int i = 0; i < 2; ++i) {
    contexts_[i] = o.contexts_[i] ? new StateContexts(*o.contexts_[i]) : NULL;
    closure_[i] =
        o.closure_[i] ? new EpsilonClosure(this, i, contexts_[i]) : NULL;
  }
}


LexiconTransducer::~LexiconTransducer() {
  delete empty_model_;
  delete splitter_;
  delete siblings_;
  for (int i = 0; i < 2; ++i) {
    delete contexts_[i];
    delete closure_[i];
  }
}

void LexiconTransducer::SetContextSize(int num_phones, int num_left_contexts,
                                       int num_right_contexts,
                                       bool center_set) {
  num_phones_ = num_phones;
  CHECK_LE(num_left_contexts, 1);
  CHECK_LE(num_right_contexts, 1);
  empty_context_ = PhoneContext(num_phones_, 0, 1);
}

void LexiconTransducer::SetCTransducer(ConstructionalTransducer *c) {
  c_ = c;
  SetContextSize(c->NumPhones(), c->NumLeftContexts(), c->NumRightContexts(),
                 c->HasCenterSets());
}

void LexiconTransducer::GetStatesForModel(const AllophoneModel *model,
                                          bool sourceState,
                                          vector<StateId> *states,
                                          bool unique) const {
  typedef LexiconTransducerImpl::ArcRefSet ArcRefSet;
  const ModelToArcMap &map = GetImpl()->ModelToArcs();
  ModelToArcMap::const_iterator arc_list = map.find(model);
  if (arc_list == map.end()) return;
  states->reserve(states->size() + arc_list->second.size());
  for (ArcRefSet::const_iterator a = arc_list->second.begin();
      a != arc_list->second.end(); ++a) {
    const Arc &arc = **a;
    states->push_back(sourceState ? arc.prevstate : arc.nextstate);
  }
  if (unique)
    RemoveDuplicates(states);
}

void LexiconTransducer::GetArcsForModel(const AllophoneModel *model,
                                        vector<ArcRef> *arcs) const {
  const ModelToArcMap &map = GetImpl()->ModelToArcs();
  ModelToArcMap::const_iterator arc_list = map.find(model);
  if (arc_list != map.end()) {
    std::copy(arc_list->second.begin(), arc_list->second.end(),
              std::back_insert_iterator< vector<ArcRef> >(*arcs));
  }
}

bool LexiconTransducer::HasModel(const AllophoneModel *model) const {
  const ModelToArcMap &map = GetImpl()->ModelToArcs();
  ModelToArcMap::const_iterator arc_list = map.find(model);
  return arc_list != map.end() && !arc_list->second.empty();
}

void LexiconTransducer::ApplyModelSplit(
    int context_pos, const ContextQuestion *question,
    AllophoneModel *old_model, int hmm_state,
    const AllophoneModel::SplitResult &new_models) {
  if (!splitter_) {
    if (shifted_)
      splitter_ = new ShiftedLexiconStateSplitter(this, num_phones_);
    else
      splitter_ = new LexiconStateSplitter(this, num_phones_);
  }
  splitter_->ApplySplit(context_pos, question, old_model, hmm_state,
      new_models);
  if (c_)
    c_->ApplyModelSplit(context_pos, question, old_model, hmm_state,
        new_models);
}

void LexiconTransducer::FinishSplit() {
  CHECK_NOTNULL(splitter_);
  splitter_->FinishSplit();
  PurgeStates();
  for (int i = 0; i < 2; ++i)
    ResetContexts(i);
}

void LexiconTransducer::ResetContexts(int pos) {
  contexts_[pos]->Clear();
  closure_[pos]->Clear();
}

void LexiconTransducer::Init(const fst::StdExpandedFst &l,
                             const ModelManager &models,
                             const map<int, int> phone_mapping,
                             int boundary_phone) {
  CHECK_GT(num_phones_, 0);
  LexiconTransducerInitializer *init = NULL;
  if (shifted_) {
    ShiftedLexiconTransducerInitializer *si =
        new ShiftedLexiconTransducerInitializer(this);
    si->SetPhoneMapping(phone_mapping);
    si->SetBoundaryPhone(boundary_phone);
    init = si;
  } else {
    init = new LexiconTransducerInitializer(this);
  }
  init->SetModels(models);
  init->Build(l);
  delete init;
  siblings_ = new LexiconStateSiblings(num_phones_);
  for (int i = 0; i < 2; ++i) {
    contexts_[i] = new StateContexts();
    closure_[i] = new EpsilonClosure(this, i, contexts_[i]);
  }
}

AbstractSplitPredictor* LexiconTransducer::CreateSplitPredictor() const {
  if (shifted_)
    return new ShiftedLexiconSplitPredictor(this);
  else
    return new LexiconSplitPredictor(this);
}

}  // namespace trainc
