// shifted_split_predictor.cc
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

#include "epsilon_closure.h"
#include "shifted_split_predictor.h"

namespace trainc {

ShiftedLexiconSplitPredictor::ShiftedLexiconSplitPredictor(
    const LexiconTransducer *l)
    : LexiconSplitPredictorBase(l), closure_(l->GetEpsilonClosure(0)),
        contexts_(closure_->GetStateContexts()) {
  CHECK(l_->IsShifted());
}

int ShiftedLexiconSplitPredictor::Count(int context_pos,
    const ContextQuestion &question,
    const AllophoneStateModel::AllophoneRefList &models, int max_new_states) {
  if (discard_absent_models_ && !ModelExists(models)) {
    return kInvalidCount;
  }
  if (context_pos == 1)
    return 0;
  vector<StateId> states;
  GetStates(context_pos, models, question, true, &states);
  StateSet all_states;
  closure_->GetUnion(states, &all_states);
  int num_new_states = 0;
  if (context_pos == 0)
    num_new_states = CountCenterSplit(all_states, question);
  else
    num_new_states = CountLeftSplit(all_states, question);
  return num_new_states;
}

int ShiftedLexiconSplitPredictor::CountCenterSplit(
    const StateSet &all_states, const ContextQuestion &question) const {
  int num_new_states = 0;
  for (StateSet::const_iterator s = all_states.begin(); s != all_states.end();
      ++s) {
    const ContextSet &context = contexts_->Context(*s);
    num_new_states += CountState(context, question);
  }
  return num_new_states;
}

int ShiftedLexiconSplitPredictor::CountLeftSplit(
    const StateSet &all_states, const ContextQuestion &question) const {
  StateSet predecessors;
  int num_new_states = 0;
  for (StateSet::const_iterator s = all_states.begin(); s != all_states.end();
      ++s) {
    ContextSet context(num_phones_);
    bool loop = false;
    num_new_states += CountPredecessors(*s, question, NULL, &context,
        &predecessors, &loop);
    for (EpsilonClosure::Iterator riter = closure_->Reachable(*s);
        !riter.Done(); riter.Next()) {
      num_new_states += CountPredecessors(riter.Value(), question, NULL,
          &context, &predecessors, &loop);
    }
    int c = CountState(context, question);
    if (c && loop) {
      for (int i = 0; i < 2; ++i) {
        ContextSet new_context(num_phones_);
        CountPredecessors(*s, question, &question.GetPhoneSet(i), &new_context,
            NULL, &loop);
        num_new_states += CountState(new_context, question);
      }
    } else {
      num_new_states += c;
    }
  }
  return num_new_states;
}

int ShiftedLexiconSplitPredictor::CountState(
    const ContextSet &state_context, const ContextQuestion &question) const {
  pair<ContextSet, ContextSet> new_context(state_context, state_context);
  for (int c = 0; c < 2; ++c)
    GetPairElement(new_context, c).Intersect(question.GetPhoneSet(c));
  return (!(new_context.first.IsEmpty() || new_context.second.IsEmpty()));
}

int ShiftedLexiconSplitPredictor::CountPredecessors(
    StateId state, const ContextQuestion &question,
    const ContextSet *filter, ContextSet *context,
    StateSet *predecessors, bool *loop) const {
  const State *s = l_->GetState(state);
  int num_new_states = 0;
  for (State::BackwardArcIterator aiter(s); !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (arc.model && (!filter || filter->HasElement(arc.ilabel))) {
      closure_->AddState(arc.prevstate);
      const ContextSet &state_context = contexts_->Context(arc.prevstate);
      context->Union(state_context);
      int c = CountState(state_context, question);
      if (!predecessors || !predecessors->count(arc.prevstate)) {
        num_new_states += c;
        if (predecessors)
          predecessors->insert(arc.prevstate);
      }
      if (c && arc.prevstate == state)
        *loop = true;
    }
  }
  return num_new_states;
}

}  // namespace trainc


