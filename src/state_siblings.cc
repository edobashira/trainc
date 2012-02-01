// state_siblings.cc
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

#include "state_siblings.h"

namespace trainc {

void LexiconStateSiblings::AddState(StateId old_state, StateId new_state,
                                    ContextId context_id,
                                    const ContextSet &new_context) {
  if (new_state >= states_.size())
    states_.resize(new_state + 1, StateDef(fst::kNoStateId, empty_context_));
  StateDef &entry = states_[new_state];
  DCHECK_EQ(entry.origin, fst::kNoStateId);
  if (old_state != new_state) {
    GetContext(old_state, &entry.context);
  } else {
    ContextPair old_context = make_pair(ContextSet(num_phones_),
        ContextSet(num_phones_));
    GetContext(old_state, &old_context);
    entry.context = old_context;
  }
  GetPairElement(entry.context, context_id).Intersect(new_context);
  entry.origin = GetOrigin(old_state);
  AddIndex(entry, new_state);
}

void LexiconStateSiblings::UpdateContext(StateId state, ContextId context_id,
                                         const ContextSet &new_context) {
  if (HasState(state)) {
    StateDef &entry = states_[state];
    if (context_id == LexiconStateSplitter::kRightContext)
      RemoveIndex(entry, state);
    GetPairElement(entry.context, context_id).Intersect(new_context);
    if (context_id == LexiconStateSplitter::kRightContext)
      AddIndex(entry, state);
  } else {
    AddState(state, state, context_id, new_context);
  }
}

void LexiconStateSiblings::RemoveState(StateId state) {
  if (HasState(state)) {
    StateDef &entry = states_[state];
    RemoveIndex(entry, state);
    entry.origin = fst::kNoStateId;
  }
}

LexiconStateSiblings::StateId LexiconStateSiblings::Find(
    StateId state, const ContextSet &left_context,
    const ContextSet &right_context) const {
  StateId result = fst::kNoStateId;
  if (states_.empty())
    return result;
  StateId origin = GetOrigin(state);
  pair<StateIndex::const_iterator, StateIndex::const_iterator> i =
      index_.equal_range(IndexKey(origin, right_context));
  for (; i.first != i.second; ++i.first) {
    DCHECK(HasState(i.first->second));
    const StateDef &entry = states_[i.first->second];
    if (left_context.IsSubSet(entry.context.first)) {
      result = i.first->second;
      break;
    }
  }
  return result;
}

void LexiconStateSiblings::GetContext(StateId state, ContextId context_id,
                                      ContextSet *context) const {
  if (HasState(state)) {
    *context = GetPairElement(states_[state].context, context_id);
  } else {
    context->Clear();
    context->Invert();
  }
}

void LexiconStateSiblings::GetContext(StateId state,
                                      ContextPair *context) const {
  if (HasState(state)) {
    *context = states_[state].context;
  } else {
    for (int i = 0; i < 2; ++i) {
      ContextSet &c = GetPairElement(*context, i);
      c.Clear();
      c.Invert();
    }
  }
}

LexiconStateSiblings::StateId LexiconStateSiblings::GetOrigin(StateId s) const {
  StateId origin = s;
  if (HasState(s))
    origin = states_[s].origin;
  return origin;
}

void LexiconStateSiblings::AddIndex(const StateDef &def, StateId s) {
  index_.insert(
      StateIndex::value_type(IndexKey(def.origin, def.context.second), s));
}

void LexiconStateSiblings::RemoveIndex(const StateDef &def, StateId s) {
  pair<StateIndex::iterator, StateIndex::iterator> i =
      index_.equal_range(IndexKey(def.origin, def.context.second));
  DCHECK(i.first != i.second);
  for (; i.first != i.second; ++i.first) {
    if (i.first->second == s) {
      index_.erase(i.first);
      break;
    }
  }
}

bool LexiconStateSiblings::HasState(StateId s) const {
  return (s < states_.size() && states_[s].origin != fst::kNoStateId);
}

} // namespace trainc
