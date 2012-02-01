// epsilon_closure.cc
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

#include "epsilon_closure.h"

namespace trainc {

void EpsilonClosure::AddState(StateId state_id) {
  if (state_id < visited_.size() && visited_[state_id])
    return;
  const State *state = l_->GetState(state_id);
  Collect(state_id, state, NULL);
}

void EpsilonClosure::Collect(StateId state_id, const State *state,
                             ContextSet *parent_context) {
  if (state_id < visited_.size() && visited_[state_id]) {
    if (parent_context)
      parent_context->Union(contexts_->Context(state_id));
    return;
  }
  ContextSet state_context = state->GetContext(forward_);
  const bool has_eps =
      forward_ ? state->NumInputEpsilons() : state->NumIncomingEpsilons();
  if (has_eps) {
    if (forward_) {
      CollectReachable(state_id, State::ConstForwardArcIterator(state),
        &state_context);
    } else {
      CollectReachable(state_id, State::BackwardArcIterator(state),
        &state_context);
    }
  }
  if (state_id >= visited_.size())
    visited_.resize(state_id + 1, false);
  visited_[state_id] = true;
  contexts_->SetContext(state_id, state_context);
  if (parent_context)
    parent_context->Union(state_context);
}

template<class Iter>
void EpsilonClosure::CollectReachable(StateId state_id, Iter aiter,
                                      ContextSet *state_context) {
  typedef typename Iter::ArcAccess ArcAccess;
  StateSet reachable;
  for (; !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (!arc.model) {
      const StateId target_id = ArcAccess::TargetState(arc);
      const State *target = l_->GetState(target_id);
      reachable.insert(target_id);
      Collect(target_id, target, state_context);
      // StateMap::const_iterator i = states_.find(arc.nextstate);
      StateMap::const_iterator i = states_.find(target_id);
      if (i != states_.end())
        reachable.insert(i->second.begin(), i->second.end());
    }
  }
  if (!reachable.empty())
    states_.insert(StateMap::value_type(state_id, reachable));
}

void EpsilonClosure::GetUnion(const vector<StateId> &states,
                              hash_set<StateId> *reachable) {
  reachable->resize(states.size());
  for (vector<StateId>::const_iterator s = states.begin(); s != states.end();
      ++s)
    AddReachable(*s, reachable);
}

void EpsilonClosure::AddReachable(StateId state, hash_set<StateId> *reachable) {
  AddState(state);
  reachable->insert(state);
  StateMap::const_iterator i = states_.find(state);
  if (i != states_.end())
    reachable->insert(i->second.begin(), i->second.end());
}

EpsilonClosure::Iterator EpsilonClosure::Reachable(StateId s) {
  AddState(s);
  StateMap::const_iterator i = states_.find(s);
  if (i != states_.end())
    return Iterator(i->second);
  else
    return Iterator();
}

}  // namespace trainc
