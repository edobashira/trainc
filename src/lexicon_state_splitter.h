// lexicon_state_splitter.h
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

#ifndef LEXICON_STATE_SPLITTER_H_
#define LEXICON_STATE_SPLITTER_H_

#include <ext/hash_map>
#include <ext/hash_set>
#include <list>
#include <set>
#include <stack>
#include <vector>
#include "lexicon_transducer.h"
#include "hash.h"

using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

namespace trainc {

// Applies model splits to a LexiconTransducer.
// Supports epsilon arcs in the transducer.
// Currently, only triphone models are supported.
class LexiconStateSplitter {
protected:
  typedef LexiconTransducer::StateId StateId;
  typedef LexiconTransducer::State State;
  typedef State::ArcRef ArcRef;
  typedef State::ArcRefSet ArcRefSet;
  typedef LexiconTransducer::Arc Arc;
public:
  LexiconStateSplitter(LexiconTransducer *l, int num_phones);
  virtual ~LexiconStateSplitter();

  virtual void ApplySplit(int context_pos, const ContextQuestion *question,
                          AllophoneModel *old_model, int hmm_state,
                          const AllophoneModel::SplitResult &new_models);

  virtual void FinishSplit();

  enum ContextId { kLeftContext = 0, kRightContext = 1 };

protected:
  class UpdateBase;
  class Update;
  template<ContextId C> class UpdateTpl;

  UpdateBase* CreateUpdate(int context_pos) const;

  void RelabelArcs(const AllophoneModel *old_model,
                   const AllophoneModel::SplitResult &new_models,
                   const Partition &new_context);

  LexiconTransducer *l_;
  UpdateBase *update_;
  int num_phones_;
};

class LexiconStateSplitter::UpdateBase {
public:
  UpdateBase(LexiconTransducer *l);
  virtual ~UpdateBase() {}

  // Add an AllophoneModel split
  void AddSplit(int context_pos, AllophoneModel *old_model,
                const AllophoneModel::SplitResult &new_models,
                const Partition &context);

  // Apply all previously added splits
  virtual void Apply() = 0;

protected:
  struct SplitStates {
    pair<StateId, StateId> states;
    pair<bool, bool> new_state;
  };
  typedef LexiconStateSplitter::State State;
  typedef LexiconStateSplitter::Arc Arc;
  typedef hash_map<const AllophoneModel*, AllophoneModel::SplitResult,
      PointerHash<const AllophoneModel> > SplitMap;
  typedef hash_map<StateId, SplitStates> SplitStateMap;
  typedef hash_set<StateId> StateSet;

  virtual void GetStates(const AllophoneModel *old_model) = 0;

  StateId CreateState(StateId old_state, int pos);
  void SetState(StateId old_state, int pos, StateId state, bool new_state);
  void RemoveIncomingArcs(StateId state, const ArcRefSet &incoming);
  void AddArcs();

  LexiconTransducer *l_;
  int num_phones_;
  SplitMap splits_;
  SplitStateMap states_;
  ContextSet context_a_, context_b_;
  Partition partition_;
  ContextId context_id_;
  std::vector<StateId> states_to_split_, remove_states_;
  std::vector<Arc> add_arcs_;
};

}  // namespace trainc

#endif  // LEXICON_STATE_SPLITTER_H_
