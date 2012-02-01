// state_splitter.h
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
// \file
// State splitting in a ConstructionalTransducer

#ifndef STATE_SPLITTER_H_
#define STATE_SPLITTER_H_

#include <set>
#include "transducer.h"

using std::set;
using std::pair;

namespace trainc {

// Performs the state splitting in a ConstructionalTransducer.
// The transducer object is set in the constructor and is modified
// using SplitHistory() and SplitFuture().
class StateSplitter {
  typedef set<State::ArcRef, ArcRefCompare> ArcRefSet;
  // typedef pair<const ContextSet&, const ContextSet&> Partition;
  typedef pair<State*, State*> SplitResult;
  typedef State::StateRefSet StateRefSet;

 public:
  // Setup state splitting for the given transducer.
  StateSplitter(ConstructionalTransducer *transducer,
                int num_left_contexts, int num_right_contexts,
                int num_phones, bool center_set)
      : transducer_(*transducer),
        num_left_contexts_(num_left_contexts),
        num_right_contexts_(num_right_contexts),
        num_phones_(num_phones),
        center_set_(center_set) {}


  // Performs a split on the left context.
  void SplitHistory(int context_pos, const AllophoneModel *old_model,
                    int hmm_state,
                    const AllophoneModel::SplitResult &new_models);

  // Performs a split on the right context.
  void SplitFuture(int context_pos, const AllophoneModel *old_model,
                   int hmm_state,
                   const AllophoneModel::SplitResult &new_models);

  // Returns true if the histories of the two states are compatible.
  // The state sequence source -> target is valid if
  //  source.center \in target.history[-1]
  //  source.history[-1] \subset target.history[-2]
  //  ...
  static bool IsValidStateSequence(const PhoneContext &source, int arc_output,
                                   const PhoneContext &target,
                                   bool have_center_set, int num_left_contexts);

 private:
  void GetPartition(int context_pos, int hmm_state,
                    const AllophoneModel::SplitResult &new_models,
                    Partition *partition) const;


  bool IsValidStateSequence(const State &source, int arc_output,
                            const State &target) const;

  bool SplitState(State *state, int context_pos,
                  const AllophoneModel *old_model,
                  const AllophoneModel::SplitResult &new_models,
                  const Partition &partition,
                  SplitResult *new_states);

  void SplitPredecessorStates(StateRefSet *states, int position,
                              const Partition &partition);

  void UpdateIncomingArcs(State *old_state, State *new_state,
                          ArcRefSet *arcs_to_remove);

  void UpdateOutgoingArcs(State *old_state, SplitResult *new_states,
                          const AllophoneModel *old_model,
                          const AllophoneModel::SplitResult &new_models,
                          ArcRefSet *arcs_to_remove);

  void RedirectLoop(State::ArcRef arc, SplitResult *new_states,
                    const AllophoneModel::SplitResult &new_models,
                    bool update_input);

  void AttachArcToNewState(const Arc &arc, const State *old_state,
                           State *new_state);

  void RelabelArc(const Arc &arc, State *new_state,
                  const AllophoneModel *model);

  ConstructionalTransducer &transducer_;
  int num_left_contexts_, num_right_contexts_;
  int num_phones_;
  bool center_set_;

  DISALLOW_COPY_AND_ASSIGN(StateSplitter);
};

inline bool StateSplitter::IsValidStateSequence(
    const PhoneContext &source, int arc_output, const PhoneContext &target,
    bool have_center_set, int num_left_contexts) {
  if (have_center_set && !target.GetContext(0).HasElement(arc_output)) {
    return false;
  }
  bool valid = true;
  for (int l = 0; l < num_left_contexts; ++l) {
    const ContextSet &source_context = source.GetContext(-l);
    const ContextSet &target_context = target.GetContext(-l - 1);
    // target_context may be empty, if target represents a CI phone
    valid = target_context.IsEmpty() ||
        source_context.IsSubSet(target_context);
    if (!valid) break;
  }
  return valid;
}


}  // namespace trainc

#endif  // STATE_SPLITTER_H_
