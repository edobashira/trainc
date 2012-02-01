// epsilon_closure.h
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
// Epsilon closure and state context for a LexiconTransducer

#ifndef EPSILON_CLOSURE_H_
#define EPSILON_CLOSURE_H_

#include <ext/hash_map>
#include "lexicon_transducer.h"
#include "debug.h"

using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

namespace trainc {

// Stores the context of a set of states.
class StateContexts {
  typedef LexiconTransducer::StateId StateId;
  typedef hash_map<StateId, ContextSet> StateContextMap;
public:

  void SetContext(StateId s, const ContextSet &c) {
    DCHECK(context_.find(s) == context_.end());
    context_.insert(StateContextMap::value_type(s, c));
  }
  const ContextSet& Context(StateId s) const {
    StateContextMap::const_iterator i = context_.find(s);
    DCHECK(i != context_.end());
    return i->second;
  }
  void Clear() {
    context_.clear();
  }
protected:
  StateContextMap context_;
};

// Explores the epsilon closure of a set of states.
// Each state in the set has to be added using AddState().
class EpsilonClosure {
public:
  typedef LexiconTransducer::State State;
  typedef LexiconTransducer::Arc Arc;
  typedef LexiconTransducer::StateId StateId;

  // If forward == false the reversed transducer is used.
  // The full state context of each state in
  // the epsilon closure is computed and stored in contexts.
  EpsilonClosure(const LexiconTransducer *l, bool forward,
                 StateContexts *contexts)
      : l_(l), forward_(forward), contexts_(contexts) {}
  void AddState(StateId state_id);
  void Clear() {
    states_.clear();
    visited_.clear();
  }
  void GetUnion(const std::vector<StateId> &states,
                hash_set<StateId> *reachable);
  void AddReachable(StateId state, hash_set<StateId> *reachable);
  class Iterator {
  public:
    Iterator() : i_(NULL, NULL), end_(NULL, NULL) {}
    Iterator(const hash_set<StateId> &set)
        : i_(set.begin()), end_(set.end()) {}
    bool Done() const { return i_ == end_; }
    void Next() { ++i_; }
    StateId Value() const { return *i_; }
  private:
    hash_set<StateId>::const_iterator i_, end_;
  };

  Iterator Reachable(StateId s);

  const StateContexts* GetStateContexts() const {
    return contexts_;
  }
private:
  void Collect(StateId state_id, const State *state,
               ContextSet *parent_context);
  template<class Iter>
  void CollectReachable(StateId state_id, Iter aiter,
                        ContextSet *state_context);
  typedef hash_set<StateId> StateSet;
  typedef hash_map<StateId, StateSet> StateMap;
  StateMap states_;
  std::vector<bool> visited_;
  const LexiconTransducer *l_;
  bool forward_;
  StateContexts *contexts_;
};

}  // namespace trainc

#endif  // EPSILON_CLOSURE_H_
