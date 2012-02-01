// state_splitter.cc
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

#include "state_splitter.h"

namespace trainc {

// Split based on the left context.
// For each arc having old_model as input, the source states are collected.
// If context_pos < -1 (i.e. the split depends on a context that is not
// the immediate predecessor of the central phone) all predecessor arcs
void StateSplitter::SplitHistory(
    int context_pos, const AllophoneModel *old_model, int hmm_state,
    const AllophoneModel::SplitResult &new_models) {
  CHECK_LE(context_pos, 0);
  StateRefSet matching_states, states_to_split;
  transducer_.GetStatesForModel(old_model, &matching_states);
  VLOG(3) << "split history: processing " << matching_states.size()
          << " states";
  if (matching_states.empty()) {
    REP(WARNING) << "no states for model " << old_model << " found";
  }
  const ContextSet empty_context(num_phones_);
  const ContextSet &context_a = (new_models.first ?
      new_models.first->GetStateModel(hmm_state)->context(context_pos) :
      empty_context);
  const ContextSet &context_b = (new_models.second ?
      new_models.second->GetStateModel(hmm_state)->context(context_pos) :
      empty_context);
  const Partition partition(context_a, context_b);
  if (context_pos < -1 || (center_set_ && context_pos == -1)) {
    SplitPredecessorStates(&matching_states, context_pos + 1, partition);
    matching_states.clear();
    transducer_.GetStatesForModel(old_model, &matching_states);
  }
  SplitResult new_states;
  for (StateRefSet::const_iterator state_iter = matching_states.begin();
       state_iter != matching_states.end(); ++state_iter) {
    State *state = *state_iter;
    bool remove_state = SplitState(state, context_pos, old_model,
                                   new_models, partition, &new_states);
    if (remove_state) {
      transducer_.RemoveState(state);
    }
  }
}

// Split based on the right context.
// A split on the right context requires only relabeling of arcs.
// (in the case of right context size limited to 1).
// Each arc having old_model as input is relabeled with the
// new model which has the output phone in its context set.
void StateSplitter::SplitFuture(
    int context_pos, const AllophoneModel *old_model, int hmm_state,
    const AllophoneModel::SplitResult &new_models) {
  CHECK_GT(context_pos, 0);
  vector<State::ArcRef> arcs_to_relabel;
  transducer_.GetArcsWithModel(old_model, &arcs_to_relabel);
  VLOG(3) << "split future: " << arcs_to_relabel.size() << " arcs to relabel";
  if (arcs_to_relabel.empty()) {
    REP(WARNING) << "no arcs matching the model to split found "
                 << old_model;
    if (new_models.first)
      VLOG(1) << new_models.first->ToString(true);
    if (new_models.second)
      VLOG(1) << new_models.second->ToString(true);
    return;
  }
  const ContextSet empty_context(num_phones_);
  const ContextSet &context_a = (new_models.first ?
      new_models.first->GetStateModel(hmm_state)->context(context_pos) :
      empty_context);
  const ContextSet &context_b = (new_models.second ?
      new_models.second->GetStateModel(hmm_state)->context(context_pos) :
      empty_context);
  for (vector<State::ArcRef>::const_iterator arc_iter = arcs_to_relabel.begin();
       arc_iter != arcs_to_relabel.end(); ++arc_iter) {
    const Arc &arc = *(*arc_iter);
    AllophoneModel *m = NULL;
    if (context_a.HasElement(arc.output()))
      m = new_models.first;
    if (context_b.HasElement(arc.output())) {
      CHECK(m == NULL);
      m = new_models.second;
    }
    CHECK_NOTNULL(m);
    transducer_.UpdateArcInput(*arc_iter, m);
  }
}


bool StateSplitter::IsValidStateSequence(
    const State &source, int arc_output, const State &target) const {
  return IsValidStateSequence(source.history(), arc_output, target.history(),
                              center_set_, num_left_contexts_);
}

// Split the given state.
// Applies the split of old_model into new_models to the given state.
// The split is defined by the position context_pos and the partition
// of context phones.
// The tuple of new states is constructed by intersecting the
// history set of the state with each of the two partitions.
// If the new states do not yet exist, they are created.
bool StateSplitter::SplitState(
    State *state, int context_pos, const AllophoneModel *old_model,
    const AllophoneModel::SplitResult &new_models, const Partition &partition,
    SplitResult *new_states) {
  const State &old_state = *state;
  // arcs are not removed directly, because they may be accessed twice
  ArcRefSet arcs_to_remove;
  bool remove_state = true;
  for (int c = 0; c < 2; ++c) {
    State* &new_state = GetPairElement(*new_states, c);
    new_state = NULL;
    PhoneContext new_history = old_state.history();
    const ContextSet &context = GetPairElement(partition, c);
    new_history.GetContextRef(context_pos)->Intersect(context);
    if (!new_history.GetContext(context_pos).IsEmpty()) {
      new_state = transducer_.GetState(new_history);
      if (!new_state) {
        new_state = transducer_.AddState(new_history);
        UpdateIncomingArcs(state, new_state, &arcs_to_remove);
      } else {
        remove_state = false;
      }
    }
  }
  UpdateOutgoingArcs(state, new_states, old_model, new_models,
                     &arcs_to_remove);
  for (ArcRefSet::const_iterator arc_iter = arcs_to_remove.begin();
       arc_iter != arcs_to_remove.end(); ++arc_iter) {
    transducer_.RemoveArc(*arc_iter);
  }
  VLOG(3) << "SplitState: " << old_state.history().ToString() << " -> " << (new_states->first ? new_states->first->history().ToString() : std::string()) << " " << (new_states->second ? new_states->second->history().ToString() : std::string());
  return remove_state;
}

void StateSplitter::SplitPredecessorStates(
    StateRefSet *states, int position, const Partition &partition) {
  StateRefSet predecessor_states;
  transducer_.GetPredecessorStatesOfSet(*states, &predecessor_states);
  if (position < -1 || (center_set_ && position == -1)) {
    // recursively split all predecessor states
    SplitPredecessorStates(&predecessor_states, position + 1, partition);
  }
  const AllophoneModel::SplitResult dummy_models(NULL, NULL);
  for (StateRefSet::iterator old_state = predecessor_states.begin();
       old_state != predecessor_states.end(); ++old_state) {
    SplitResult split_states;
    bool remove_state = SplitState(*old_state, position, NULL, dummy_models,
                                   partition, &split_states);
    if (remove_state) {
      transducer_.RemoveState(*old_state);
    }
    StateRefSet::iterator s = states->find(*old_state);
    if (s != states->end()) {
      // loop arc split.
      // replace state by the two new states.
      states->erase(s);
      for (int s = 0; s < 2; ++s) {
        State *new_state = GetPairElement(split_states, s);
        if (new_state) {
          states->insert(new_state);
        }
      }
    }
  }
}

void StateSplitter::UpdateIncomingArcs(
    State *old_state, State *new_state, ArcRefSet *arcs_to_remove) {
  // TODO(rybach): check if we need a copy of the incoming arcs here.
  vector<State::ArcRef> incoming_arcs;
  for (State::ArcRefList::iterator arc_iter =
           old_state->GetIncomingArcsRef()->begin();
       arc_iter != old_state->GetIncomingArcsRef()->end(); ++arc_iter)
    incoming_arcs.push_back(*arc_iter);
  for (vector<State::ArcRef>::iterator arc_iter = incoming_arcs.begin();
       arc_iter != incoming_arcs.end(); ++arc_iter) {
    State::ArcRef arc = *arc_iter;
    DCHECK(arc->target() == old_state);
    State *source_state = arc->source();
    if (source_state != old_state) {
      // loops will be updated in UpdateOutgoingArcs
      if (IsValidStateSequence(*source_state, arc->output(), *new_state)) {
        transducer_.AddArc(source_state, new_state,
                           arc->input(), arc->output());
        arcs_to_remove->insert(*arc_iter);
      }
    }
  }
}

void StateSplitter::UpdateOutgoingArcs(
    State *old_state, SplitResult *new_states, const AllophoneModel *old_model,
    const AllophoneModel::SplitResult &new_models,
    ArcRefSet *arcs_to_remove) {
  // make a copy of the arcs, because we will modify them
  vector<State::ArcRef> outgoing_arcs;
  for (State::ArcList::iterator arc_iter = old_state->GetArcsRef()->begin();
       arc_iter != old_state->GetArcsRef()->end(); ++arc_iter)
    outgoing_arcs.push_back(arc_iter);
  for (vector<State::ArcRef>::const_iterator arc_iter = outgoing_arcs.begin();
       arc_iter != outgoing_arcs.end(); ++arc_iter) {
    const Arc &arc = *(*arc_iter);
    bool remove_arc = false;
    if (arc.target() == old_state) {
      const bool update_model = (arc.input() == old_model);
      RedirectLoop(*arc_iter, new_states, new_models, update_model);
      remove_arc = true;
    } else {
      if (arc.input() != old_model) {
        // don't relabel arc, just copy to both new states
        for (int s = 0; s < 2; ++s) {
          State *state = GetPairElement(*new_states, s);
          if (state && state != old_state) {
            AttachArcToNewState(arc, old_state, state);
            remove_arc = true;
          }
        }
      } else {
        for (int s = 0; s < 2; ++s) {
          State *state = GetPairElement(*new_states, s);
          if (state) {
            const AllophoneModel *model = GetPairElement(new_models, s);
            RelabelArc(arc, state, model);
            remove_arc = true;
          }
        }
      }  // if arc.input() != old_model
    }  // if arc.target() == old_state
    if (remove_arc)
      arcs_to_remove->insert(*arc_iter);
  }  // for
}

void StateSplitter::RedirectLoop(
    State::ArcRef arc, SplitResult *new_states,
    const AllophoneModel::SplitResult &new_models, bool update_input) {
  for (int s = 0; s < 2; ++s) {
    State *source = GetPairElement(*new_states, s);
    if (!source) continue;
    AllophoneModel *model = GetPairElement(new_models, s);
    State *arc_target = NULL;
    for (int t = 0; t < 2; ++t) {
      State *target = GetPairElement(*new_states, t);
      if (!target) continue;
      if (IsValidStateSequence(*source, arc->output(), *target)) {
        arc_target = target;
      }
    }
    DCHECK(arc_target != NULL);
    const AllophoneModel *arc_input = (update_input ? model : arc->input());
    transducer_.AddArc(source, arc_target, arc_input, arc->output());
  }
}

void StateSplitter::AttachArcToNewState(
    const Arc &arc, const State *old_state, State *new_state) {
  State *new_target = arc.target();
  if (new_target == old_state) new_target = new_state;
  transducer_.AddArc(new_state, new_target, arc.input(), arc.output());
}

void StateSplitter::RelabelArc(
    const Arc &arc, State *new_state, const AllophoneModel *model) {
  transducer_.AddArc(new_state, arc.target(), model, arc.output());
}

}  // namespace trainc
