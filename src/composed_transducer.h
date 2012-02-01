// composed_transducer.h
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
//
// \file required state counting using a composed transducer
//

#ifndef COMPOSED_TRANSDUCER_H_
#define COMPOSED_TRANSDUCER_H_

#include "fst/fst-decl.h"
#include "fst/compose-filter.h"
#include "fst/matcher.h"
#include "map_statetable.h"
#include "transducer.h"

namespace trainc {

class AbstractSplitPredictor;
class FstInterface;
class StateObserver;
class ComposedStatePredictor;

// Intermediate transducer composed of the C transducer and
// an other transducer, typically a lexicon transducer L
class ComposedTransducer : public StateCountingTransducer {
public:
  typedef fst::StdArc::StateId StateId;
  typedef std::map<StateId, ContextSet> PredecessorList;

  ComposedTransducer();
  virtual ~ComposedTransducer();

  void SetBoundaryPhone(int phone) { boundary_phone_ = phone; }

  // Set the C transducer to be composed.
  // Ownership stays at caller.
  // SetBoundaryPhone() has to be called before.
  void SetCTransducer(ConstructionalTransducer *c);

  // Set the right hand side transducer for the composition.
  // The transducer must have phone labels on the input side.
  void SetLTransducer(const fst::StdVectorFst &l);

  // Initialize composed transducer.
  // Must be called after SetCTransducer(), SetLTransducer() and before
  // all other operations.
  void Init();

  int NumStates() const;

  void ApplyModelSplit(int context_pos, const ContextQuestion *question,
                       AllophoneModel *old_model, int hmm_state,
                       const AllophoneModel::SplitResult &new_models);

  void FinishSplit();

  // process AddState event from the C transducer
  void StateAdded(const State *s);

  // process RemoveState event from the C transducer
  void StateRemoved(const State *s);

  // process add/remove arc event from the C transducer
  void ArcUpdate() { need_update_ = true; }

  AbstractSplitPredictor* CreateSplitPredictor() const;
private:
  typedef fst::Matcher<fst::StdFst> Matcher;
  typedef fst::SequenceComposeFilter<Matcher> Filter;
  typedef MapComposeStateTable<fst::StdArc, Filter::FilterState> StateTable;

  void Update();

  ConstructionalTransducer *c_;
  FstInterface *cfst_;
  fst::StdVectorFst *lfst_;
  fst::StdComposeFst *cl_;
  StateTable *composed_states_;
  StateObserver *observer_;
  int boundary_phone_;
  int num_phones_;
  int num_states_;
  int num_left_contexts_;
  bool center_sets_;
  bool need_update_;
  std::vector<ContextSet> incoming_;
  std::vector<PredecessorList> cl_predecessors_;
  friend class ComposedStatePredictor;
};


}  // namespace trainc

#endif  // COMPOSED_TRANSDUCER_H_
