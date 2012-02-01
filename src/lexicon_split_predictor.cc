// lexicon_split_predictor.cc
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

#include "epsilon_closure.h"
#include "lexicon_split_predictor.h"
#include "lexicon_state_splitter.h"
#include "state_siblings.h"
#include "util.h"

namespace trainc {

LexiconSplitPredictorBase::LexiconSplitPredictorBase(const LexiconTransducer *l) :
  l_(l), num_phones_(l_->NumPhones()), discard_absent_models_(true) {}

void LexiconSplitPredictorBase::GetStates(
    int context_pos, const AllophoneStateModel::AllophoneRefList &models,
    const ContextQuestion &question, bool source_state,
    vector<StateId> *states) const {
  typedef AllophoneStateModel::AllophoneRefList::const_iterator ModelIter;
  DCHECK_GT(models.size(), 0);
  ContextSet context(question.GetPhoneSet(false).Capacity());
  for (ModelIter model = models.begin(); model != models.end(); ++model) {
    (*model)->GetCommonContext(context_pos, &context);
    bool is_empty = false;
    for (int c = 0; c < 2; ++c) {
      ContextSet nc = context;
      nc.Intersect(question.GetPhoneSet(c));
      is_empty |= nc.IsEmpty();
    }
    if (!is_empty)
      l_->GetStatesForModel(*model, source_state, states, false);
  }
  RemoveDuplicates(states);
}

bool LexiconSplitPredictorBase::ModelExists(
    const AllophoneStateModel::AllophoneRefList &models) const {
  typedef AllophoneStateModel::AllophoneRefList::const_iterator ModelIter;
  for (ModelIter model = models.begin(); model != models.end(); ++model) {
    if (l_->HasModel(*model))
      return true;
  }
  return false;
}

// ======================================================================

LexiconSplitPredictor::LexiconSplitPredictor(const LexiconTransducer *l) :
  LexiconSplitPredictorBase(l), siblings_(l->GetSiblings()),
  deterministic_(l->DeterministicSplit()) {
  CHECK(!l->IsShifted());
}

void LexiconSplitPredictor::Init() {
  know_state_.clear();
  has_other_model_.clear();
}

bool LexiconSplitPredictor::HasOtherModels(StateId state_id,
                                           const ModelSet &models) {
  if (state_id < know_state_.size()) {
    if (know_state_[state_id])
      return has_other_model_[state_id];
  } else {
    know_state_.resize(state_id + 1, false);
    has_other_model_.resize(state_id + 1, false);
  }
  know_state_[state_id] = true;
  const State *state = l_->GetState(state_id);
  State::BackwardArcIterator aiter(state);
  for (; !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (arc.model && !models.count(arc.model)) {
      has_other_model_[state_id] = true;
      return true;
    }
  }
  has_other_model_[state_id] = false;
  return false;
}

bool LexiconSplitPredictor::KeepState(StateId state_id,
                                      const ModelSet &models) {
  if (HasOtherModels(state_id, models))
    return true;
  EpsilonClosure *closure = l_->GetEpsilonClosure(0);
  for (EpsilonClosure::Iterator si = closure->Reachable(state_id); !si.Done();
      si.Next()) {
    if (HasOtherModels(si.Value(), models))
      return true;
  }
  return false;
}

int LexiconSplitPredictor::CountState(
    const StateSet &all_states,
    StateId state_id, const ContextSet &state_context,
    const ContextQuestion &question, bool split_right,
    const ModelSet &models) {
  int num_new_states = 0;
  pair<ContextSet, ContextSet> new_context(state_context, state_context);
  const ContextId context_id = static_cast<ContextId>(split_right);
  for (int c = 0; c < 2; ++c)
    GetPairElement(new_context, c).Intersect(question.GetPhoneSet(c));
  if (!(new_context.first.IsEmpty() || new_context.second.IsEmpty())) {
    if (split_right && deterministic_) {
      AddStates(all_states, state_id, context_id, question);
      if (!KeepState(state_id, models))
        --num_new_states;
    } else {
      ++num_new_states;
    }
  }
  return num_new_states;
}

void LexiconSplitPredictor::AddStates(const StateSet &all_states,
                                      StateId state_id, ContextId context_id,
                                      const ContextQuestion &question) {
  DCHECK_EQ(context_id, LexiconStateSplitter::kRightContext);
  EpsilonClosure *bwd_closure = l_->GetEpsilonClosure(0);
  bwd_closure->AddState(state_id);
  const StateContexts *bwd_context = bwd_closure->GetStateContexts();
  const ContextSet &left_context = bwd_context->Context(state_id);
  ContextPair max_context = std::make_pair(ContextSet(num_phones_),
      ContextSet(num_phones_));
  siblings_->GetContext(state_id, &max_context);
  const ContextSet &right_context = max_context.second;
  for (int c = 0; c < 2; ++c) {
    StateId sibling = fst::kNoStateId;
    ContextSet context = right_context;
    context.Intersect(question.GetPhoneSet(c));
    sibling = siblings_->Find(state_id, left_context, context);
    if (sibling == fst::kNoStateId) {
      sibling = siblings_->Find(state_id, left_context, right_context);
      if (sibling != fst::kNoStateId &&
          (sibling == state_id || !all_states.count(sibling)))
        sibling = fst::kNoStateId;
    }
    if (sibling == fst::kNoStateId) {
      new_states_.insert(
          NewState(siblings_->GetOrigin(state_id),
              make_pair(max_context.first, context)));
    }
  }
}

int LexiconSplitPredictor::Count(
    int context_pos, const ContextQuestion &question,
    const AllophoneStateModel::AllophoneRefList &models,
    int max_new_states) {
  if (discard_absent_models_ && !ModelExists(models)) {
    return kInvalidCount;
  }
  if (context_pos == 0)
    return 0;
  const bool split_right = context_pos == 1;
  vector<StateId> states;
  GetStates(context_pos, models, question, context_pos == -1, &states);
  EpsilonClosure *closure = l_->GetEpsilonClosure(split_right);
  const StateContexts *contexts = closure->GetStateContexts();
  StateSet all_states;
  closure->GetUnion(states, &all_states);
  int num_new_states = 0;
  ModelSet split_models;
  if (split_right) {
    std::copy(models.begin(), models.end(),
        std::inserter(split_models, split_models.begin()));
  }

  for (StateSet::const_iterator si = all_states.begin(); si != all_states.end();
      ++si) {
    num_new_states += CountState(all_states, *si, contexts->Context(*si),
        question, split_right, split_models);
  }
  num_new_states += new_states_.size();
  new_states_.clear();
  return num_new_states;
}

}  // namespace trainc
