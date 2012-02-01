// state_siblings.h
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
// Groups of states in an un-shifted LexiconTransducer

#ifndef STATE_SIBLINGS_H_
#define STATE_SIBLINGS_H_

#include <ext/hash_map>
#include <vector>
#include "lexicon_transducer.h"
#include "lexicon_state_splitter.h"

using __gnu_cxx::hash_multimap;

namespace trainc {

// Keeps track of the silblings of a state.
// Two states are siblings if they have been created by splitting the same
// state in the initial transducer or one of its siblings.
// The maximum allowed left and right context set of each state is stored, too.
// Siblings are only required for right context splits.
// For efficient lookup of matching siblings for a given state and context,
// and index (origin, right-context) -> state is maintained.
class LexiconStateSiblings {
public:
  typedef LexiconStateSplitter::ContextId ContextId;
  typedef LexiconTransducer::State State;
  typedef LexiconTransducer::Arc Arc;
  typedef LexiconTransducer::StateId StateId;
  typedef pair<ContextSet, ContextSet> ContextPair;

  LexiconStateSiblings(int num_phones) : num_phones_(num_phones),
      empty_context_(ContextSet(num_phones_), ContextSet(num_phones_)) {}

  void RemoveState(StateId s);
  void AddState(StateId old_state, StateId new_state, ContextId context_id,
                const ContextSet &new_context);
  void UpdateContext(StateId state, ContextId context_id,
                     const ContextSet &new_context);
  StateId Find(StateId state, const ContextSet &left_context,
               const ContextSet &right_context) const;
  void GetContext(StateId state, ContextId context_id,
                  ContextSet *context) const;
  void GetContext(StateId state, ContextPair *context) const;
  StateId GetOrigin(StateId s) const;

private:
  class IndexKey {
  public:
    StateId state;
    ContextSet context;
    IndexKey(StateId s, const ContextSet &c) :
      state(s), context(c) {}
    size_t HashValue() const {
      size_t h = state;
      HashCombine(h, context.HashValue());
      return h;
    }
    bool IsEqual(const IndexKey &o) const {
      return state == o.state && context.IsEqual(o.context);
    }
  };
  struct StateDef {
    StateId origin;
    ContextPair context;
    StateDef(StateId o, const ContextPair &c) : origin(o), context(c) {}
  };
  typedef hash_multimap<IndexKey, StateId,
      Hash<IndexKey>, Equal<IndexKey> > StateIndex;
  typedef std::vector<StateDef> StateList;

  void AddIndex(const StateDef &def, StateId s);
  void RemoveIndex(const StateDef &def, StateId s);
  bool HasState(StateId s) const;
  StateList states_;
  StateIndex index_;
  int num_phones_;
  const ContextPair empty_context_;
};

}  // namespace trainc

#endif  // STATE_SIBLINGS_H_
