// shifted_state_splitter.cc
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
//

#include <ext/hash_map>
#include "epsilon_closure.h"
#include "shifted_state_splitter.h"
#include "util.h"

using std::vector;
using __gnu_cxx::hash_map;

namespace trainc {

class ShiftedLexiconStateSplitter::Update : public LexiconStateSplitter::UpdateBase {
  typedef LexiconStateSplitter::UpdateBase UpdateBase;
public:
  Update(LexiconTransducer *l)
    : UpdateBase(l), closure_(l->GetEpsilonClosure(0)),
        contexts_(l_->GetStateContexts(0)) {}
  virtual ~Update() {}
  void Apply();

private:
  void ApplyLeftSplit(StateSet *states);
  void GetStates(const AllophoneModel *model) {
    l_->GetStatesForModel(model, true, &states_to_split_, false);
  }
  void ModifyArcs(const StateSet &states) {}
  void ModifyArcs(const StateSet &states, bool update_models);
  void UpdateIncomingArcs(const StateSet &states);
  void UpdateIncomingArc(const Arc &arc, const SplitStates &new_states);
  void UpdateOutgoingArcs(const StateSet &states, bool update_models);
  void UpdateModel(const Arc &arc, const AllophoneModel::SplitResult &new_models,
                 const SplitStates *new_states,
                 const ContextSet &state_context);
  void CopyArc(const Arc &arc, const SplitStates &new_states);
  void Split(const StateSet &states);
  void SplitState(StateId state, const ContextSet &state_context);
  void FindPredecessors(const StateSet &states, StateSet *predecessors);
  void RemoveStates();
  void UpdateStates(const StateSet &states);
  void UpdateSplitStates(const SplitStateMap &split_states,
                         StateSet *states);
  void CollectPredecessorContext(StateId state, ContextSet *context,
      vector<StateId> *predecessors) const;
  int GetValidStatePart(StateId s) {
    return valid_state_part_.count(s);
  }
  void SetValidStatePart(StateId s, int part) {
    if (part) valid_state_part_.insert(s);
  }
  static const ContextId kCenterContext;
  EpsilonClosure *closure_;
  const StateContexts *contexts_;
  hash_set<StateId> valid_state_part_;
};

const ShiftedLexiconStateSplitter::ContextId
ShiftedLexiconStateSplitter::Update::kCenterContext =
    LexiconStateSplitter::kRightContext;

void ShiftedLexiconStateSplitter::Update::Apply() {
  RemoveDuplicates(&states_to_split_);
  StateSet states;
  closure_->GetUnion(states_to_split_, &states);
  if (context_id_ == kCenterContext) {
    Split(states);
    ModifyArcs(states, true);
  } else {
    ApplyLeftSplit(&states);
  }
  RemoveStates();
  UpdateStates(states);
}

void ShiftedLexiconStateSplitter::Update::ApplyLeftSplit(StateSet *states) {
  CHECK_EQ(context_id_, kLeftContext);
  StateSet predecessors;
  FindPredecessors(*states, &predecessors);
  SplitStateMap direct_states;
  hash_set<StateId> valid_state_part;
  states_.swap(direct_states);
  valid_state_part_.swap(valid_state_part);
  context_id_ = kCenterContext;
  Split(predecessors);
  ModifyArcs(predecessors, false);
  RemoveStates();
  UpdateStates(predecessors);
  if (!states_.empty()) {
    // states have changed, update the closures
    l_->ResetContexts(kLeftContext);
  }
  states_.swap(direct_states);
  valid_state_part_.swap(valid_state_part);
  context_id_ = kLeftContext;
  UpdateSplitStates(direct_states, states);
  ModifyArcs(*states, true);
}

void ShiftedLexiconStateSplitter::Update::RemoveStates() {
  for (SplitStateMap::const_iterator s = states_.begin(); s != states_.end();
    ++s)
    l_->RemoveState(s->first);
}

void ShiftedLexiconStateSplitter::Update::UpdateStates(const StateSet &states) {
  for (StateSet::const_iterator s = states.begin(); s != states.end();
      ++s) {
    SplitStateMap::const_iterator split = states_.find(*s);
    if (split != states_.end()) {
      // state has been split
      for (int c = 0; c < 2; ++c) {
        StateId ns = GetPairElement(split->second.states, c);
        if (ns != fst::kNoStateId)
          l_->GetStateRef(ns)->UpdateContext();
      }
    } else {
      l_->GetStateRef(*s)->UpdateContext();
    }
  }
}

void ShiftedLexiconStateSplitter::Update::FindPredecessors(
    const StateSet &states, StateSet *predecessors) {
  vector<StateId> pre;
  for (StateSet::const_iterator s = states.begin(); s != states.end(); ++s) {
    ContextSet context(num_phones_);
    CollectPredecessorContext(*s, &context, &pre);
    for (EpsilonClosure::Iterator riter = closure_->Reachable(*s);
        !riter.Done(); riter.Next())
      CollectPredecessorContext(riter.Value(), &context, &pre);
    SplitState(*s, context);
  }
  if (predecessors) {
    RemoveDuplicates(&pre);
    closure_->GetUnion(pre, predecessors);
  }
}

void ShiftedLexiconStateSplitter::Update::CollectPredecessorContext(
    StateId state, ContextSet *context, vector<StateId> *predecessors) const {
  const State *s = l_->GetState(state);
  for (State::BackwardArcIterator aiter(s); !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (arc.model) {
      predecessors->push_back(arc.prevstate);
      closure_->AddState(arc.prevstate);
      context->Union(contexts_->Context(arc.prevstate));
    }
  }
}

void ShiftedLexiconStateSplitter::Update::UpdateSplitStates(
  const SplitStateMap &split_states, StateSet *states) {
  StateSet new_states;
  for (SplitStateMap::const_iterator s = split_states.begin();
      s != split_states.end(); ++s) {
    if (states->count(s->first)) {
      states->erase(s->first);
      SplitStateMap::iterator ns = states_.find(s->first);
      if (ns != states_.end()) {
        states_.erase(ns);
        for (int c = 0; c < 2; ++c) {
          StateId split_state = GetPairElement(ns->second.states, c);
          if (split_state != fst::kNoStateId)
            l_->RemoveState(split_state);
        }
      }
      for (int c = 0; c < 2; ++c) {
        StateId new_state = GetPairElement(s->second.states, c);
        if (new_state != fst::kNoStateId) {
          closure_->AddReachable(new_state, states);
          new_states.insert(new_state);
        }
      }
    }
  }
  FindPredecessors(new_states, NULL);
  for (StateSet::const_iterator s = states->begin(); s != states->end(); ++s)
    closure_->AddState(*s);
}

void ShiftedLexiconStateSplitter::Update::Split(const StateSet &states) {
  const StateContexts *contexts = l_->GetStateContexts(kLeftContext);
  for (StateSet::const_iterator s = states.begin(); s != states.end(); ++s) {
    SplitState(*s, contexts->Context(*s));
  }
}

void ShiftedLexiconStateSplitter::Update::SplitState(
  StateId state, const ContextSet &state_context) {
  ContextSet intersect_a = partition_.first, intersect_b = partition_.second;
  pair<ContextSet, ContextSet> new_state_context(partition_.first,
      partition_.second);
  for (int c = 0; c < 2; ++c)
    GetPairElement(new_state_context, c).Intersect(state_context);
  const bool is_valid = new_state_context.first.IsEmpty() ||
      new_state_context.second.IsEmpty();
  if (!is_valid) {
    for (int c = 0; c < 2; ++c) {
      const ContextSet &new_context = GetPairElement(new_state_context, c);
      if (!new_context.IsEmpty()) {
        CreateState(state, c);
      }
    }
  } else {
    SetValidStatePart(state, new_state_context.first.IsEmpty());
  }
}

void ShiftedLexiconStateSplitter::Update::ModifyArcs(const StateSet &states,
                                                     bool update_models) {
  UpdateIncomingArcs(states);
  UpdateOutgoingArcs(states, update_models);
}

void ShiftedLexiconStateSplitter::Update::UpdateIncomingArcs(
  const StateSet &states) {
  for (StateSet::const_iterator s = states.begin(); s != states.end(); ++s) {
    const State *old_state = l_->GetState(*s);
    SplitStateMap::const_iterator new_states = states_.find(*s);
    if (new_states == states_.end())
      continue;
    const SplitStates &split_states = new_states->second;
    for (LexiconState::BackwardArcIterator aiter(old_state); !aiter.Done();
        aiter.Next()) {
      UpdateIncomingArc(aiter.Value(), split_states);
    }
    AddArcs();
    RemoveIncomingArcs(fst::kNoStateId, old_state->GetIncomingArcs());
  }
}

void ShiftedLexiconStateSplitter::Update::UpdateIncomingArc(
    const Arc &arc, const SplitStates &new_states) {
  SplitStateMap::const_iterator src = states_.find(arc.prevstate);
  for (int c = 0; c < 2; ++c) {
    const ContextSet &context = GetPairElement(partition_, c);
    StateId new_state = GetPairElement(new_states.states, c);
    Arc new_arc = arc;
    new_arc.nextstate = new_state;
    bool is_valid_arc = false;
    if (!arc.model) {
      if (src != states_.end()) {
        new_arc.prevstate = GetPairElement(src->second.states, c);
        is_valid_arc = true;
      } else {
        is_valid_arc = (GetValidStatePart(arc.prevstate) == c);
      }
    } else {
      if (context_id_ == kCenterContext) {
        is_valid_arc = context.HasElement(arc.ilabel);
      } else {
        closure_->AddState(arc.prevstate);
        const ContextSet &state_context = contexts_->Context(arc.prevstate);
        is_valid_arc = state_context.IsSubSet(context);
      }
    }
    if (is_valid_arc)
      add_arcs_.push_back(new_arc);
  }
}

void ShiftedLexiconStateSplitter::Update::UpdateOutgoingArcs(
  const StateSet &states, bool update_models) {
  vector<State::ArcRef> remove_arcs;
  for (StateSet::const_iterator s = states.begin(); s != states.end(); ++s) {
    State *old_state = l_->GetStateRef(*s);
    const ContextSet *state_context = NULL;
    if (context_id_ == kCenterContext) {
      state_context = &contexts_->Context(*s);
    }
    SplitStateMap::const_iterator new_states = states_.find(*s);
    const SplitStates *split_states = NULL;
    if (new_states != states_.end())
      split_states = &new_states->second;
    for (LexiconState::ForwardArcIterator aiter(old_state); !aiter.Done();
        aiter.Next()) {
      const Arc &arc = aiter.Value();
      if (context_id_ == kLeftContext) {
        closure_->AddState(arc.prevstate);
        state_context = &contexts_->Context(arc.prevstate);
      }
      SplitMap::const_iterator split = splits_.find(arc.model);
      bool remove_arc = true;
      if (split != splits_.end() && update_models) {
        UpdateModel(arc, split->second, split_states, *state_context);
      } else if (split_states) {
        CopyArc(arc, *split_states);
      } else {
        remove_arc = false;
      }
      if (remove_arc)
        remove_arcs.push_back(aiter.Ref());
    }
    AddArcs();
  }
  for (vector<State::ArcRef>::const_iterator a = remove_arcs.begin();
    a != remove_arcs.end(); ++a) {
    l_->RemoveArc((*a)->prevstate, *a);
  }
}

void ShiftedLexiconStateSplitter::Update::UpdateModel(
  const Arc &arc, const AllophoneModel::SplitResult &new_models,
  const SplitStates *new_states, const ContextSet &state_context) {
  for (int c = 0; c < 2; ++c) {
    Arc new_arc = arc;
    const AllophoneModel *new_model = GetPairElement(new_models, c);
    if (!new_model) continue;
    new_arc.model = new_model;
    bool valid_arc = true;
    if (new_states) {
      new_arc.prevstate = GetPairElement(new_states->states, c);
    } else {
      if (context_id_ == kCenterContext)
        valid_arc = state_context.IsSubSet(GetPairElement(partition_, c));
      else
        valid_arc = (GetValidStatePart(arc.prevstate) == c);
    }
    if (valid_arc)
      add_arcs_.push_back(new_arc);
  }
}

void ShiftedLexiconStateSplitter::Update::CopyArc(
    const Arc &arc, const SplitStates &new_states) {
  for (int c = 0; c < 2; ++c) {
    Arc new_arc = arc;
    new_arc.prevstate = GetPairElement(new_states.states, c);
    add_arcs_.push_back(new_arc);
  }
}

// ==================================================

ShiftedLexiconStateSplitter::ShiftedLexiconStateSplitter(LexiconTransducer *l,
                                                         int num_phones)
    : LexiconStateSplitter(l, num_phones), update_(NULL) {
  CHECK(l_->IsShifted());
}


ShiftedLexiconStateSplitter::~ShiftedLexiconStateSplitter() {
  delete update_;
}

void ShiftedLexiconStateSplitter::FinishSplit() {
  if (update_) {
    update_->Apply();
    delete update_;
    update_ = NULL;
  }
}

void ShiftedLexiconStateSplitter::ApplySplit(
    int context_pos, const ContextQuestion *question,
    AllophoneModel *old_model, int hmm_state,
    const AllophoneModel::SplitResult &new_models) {
  ContextSet context_a(num_phones_), context_b(num_phones_);
  Partition partition(question->GetPhoneSet(0), question->GetPhoneSet(1));
  if (context_pos == 1) {
    RelabelArcs(old_model, new_models, partition);
  } else {
    if (!update_) update_ = new Update(l_);
    update_->AddSplit(context_pos, old_model, new_models, partition);
  }
}

}  // namespace trainc
