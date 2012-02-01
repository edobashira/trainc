// split_predictor.cc
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

#include <sstream>
#include <limits>
#include "state_splitter.h"
#include "split_predictor.h"
#include "composed_transducer.h"
#include "fst_interface.h"

namespace trainc {

const int AbstractSplitPredictor::kInvalidCount =
    std::numeric_limits<int>::min();

void SplitPredictor::GetHistories(
  const StateRefSet &states, HistorySet *histories) const {
  histories->clear();
  histories->resize(states.size());
  for (StateRefSet::const_iterator s = states.begin(); s != states.end(); ++s)
    histories->insert((*s)->history());
}

// Fill closure_ with the predecessors of states.
// closure_[0] = states
// closure_[i] = predecessors(closure_[i-1])
void SplitPredictor::GetPredecessors(
    int context_pos, const StateRefSet &states) {
  int context_size = 1 - context_pos;
  closure_.resize(context_size);
  GetHistories(states, &closure_[0]);
  StateRefSet predecessors;
  StateRefSet current_set = states;
  for (int pos = context_pos + 1, i = 1; pos <= 0; ++pos, ++i) {
    if (pos == 0 && !center_set_) {
      closure_[i].clear();
    } else {
      predecessors.clear();
      transducer_.GetPredecessorStatesOfSet(current_set, &predecessors);
      DCHECK(i < closure_.size());
      GetHistories(predecessors, &closure_[i]);
      current_set.swap(predecessors);
    }
  }
}

// Check if state is in closure_[position-1 .. 0] and if so replace it
// by new_histories[0/1] if valid_states[0/1].
void SplitPredictor::UpdateSuccessors(
    int position, const PhoneContext &state,
    const pair<PhoneContext, PhoneContext> &new_histories,
    const pair<bool, bool> &valid_states) {
  for (int j = position - 1; j >= 0; --j) {
    DCHECK_LT(j, closure_.size());
    HistorySet &successors = closure_[j];
    HistorySet::iterator sh = successors.find(state);
    if (sh != successors.end()) {
      successors.erase(sh);
      for (int c = 0; c < 2; ++c) {
        if (GetPairElement(valid_states, c)) {
          successors.insert(GetPairElement(new_histories, c));
        }
      }
    }
  }
}

void SplitPredictor::GetStates(
    int context_pos, const ContextQuestion &question,
    const AllophoneStateModel::AllophoneRefList &models,
    StateRefSet *states) const {
  typedef AllophoneStateModel::AllophoneRefList::const_iterator ModelIter;
  DCHECK_GT(models.size(), 0);
  for (ModelIter model = models.begin(); model != models.end(); ++model)
    transducer_.GetStatesForModel(*model, states);
}


int SplitPredictor::Count(
    int context_pos, const ContextQuestion &question,
    const AllophoneStateModel::AllophoneRefList &models,
    int max_new_states, StateUpdates *updates) {
  if (context_pos == 1)
    return 0;
  Reset();
  StateRefSet states;
  GetStates(context_pos, question, models, &states);
  GetPredecessors(context_pos, states);
  int num_states = 0;
  for (int i = closure_.size() - 1, pos = 0; i >= 0; --i, --pos) {
    const HistorySet &set = closure_[i];
    for (HistorySet::const_iterator h = set.begin(); h != set.end(); ++h) {
      int new_states = 0;
      pair<PhoneContext, PhoneContext> new_histories(*h, *h);
      pair<bool, bool> valid_states(false, false);
      for (int c = 0; c < 2; ++c) {
        PhoneContext &new_history = GetPairElement(new_histories, c);
        bool &valid_state = GetPairElement(valid_states, c);
        new_history.SetContext(pos, h->GetContext(pos));
        new_history.GetContextRef(pos)->Intersect(question.GetPhoneSet(c));
        if (!new_history.GetContext(pos).IsEmpty() &&
            transducer_.GetState(new_history) == NULL) {
          valid_state = true;
          ++new_states;
        }
      }
      if (new_states) {
        UpdateSuccessors(i, *h, new_histories, valid_states);
        // number of new states - deleted old state
        num_states += (new_states - 1);
        if (max_new_states && num_states > max_new_states)
          return max_new_states;
        if (updates) {
          updates->push_back(
              StateUpdates::value_type(*h, new_histories, valid_states));
        }
      }
    }
  }
  return num_states;
}

void SplitPredictor::Reset() {
  closure_.clear();
}

// ========================================================

ComposedStatePredictor::ComposedStatePredictor(const ComposedTransducer &cl)
    : cl_(cl), count_c_(NULL), vc_state_id_offset_(0) {
  // TODO(rybach): avoid cast here
  count_c_ = static_cast<SplitPredictor*>(cl_.c_->CreateSplitPredictor());
}

ComposedStatePredictor::~ComposedStatePredictor() {
  delete count_c_;
}

int ComposedStatePredictor::Count(
    int context_pos, const ContextQuestion &question,
    const AllophoneStateModel::AllophoneRefList &models,
    int max_new_states) {
  Reset();
  StateUpdates updates;
  count_c_->Count(context_pos, question, models, 0, &updates);
  int new_states = 0;
  std::set<StateId> cl_states;
  typedef StateUpdates::const_iterator UpdateIter;
  for (UpdateIter u = updates.begin(); u != updates.end(); ++u) {
    for (int c = 0; c < 2; ++c) {
      if (GetPairElement(u->valid_states, c)) {
        const PhoneContext &new_state = GetPairElement(u->new_states, c);
        new_states += NumNewStates(u->original, new_state, &cl_states);
        if (max_new_states && new_states > max_new_states)
          return max_new_states;
      }
    }
  }
  return new_states - cl_states.size();
}

// reset all internal data structures
void ComposedStatePredictor::Reset() {
  vcl_states_.Clear();
  vc_states_.clear();
  vc_state_history_.clear();
  vcl_state_origin_.clear();
  split_clstates_.clear();
  vcl_states_.SetStateIdOffset(cl_.composed_states_->MaxId() + 1);
  vc_state_id_offset_ = cl_.cfst_->MaxStateId() + 1;
}

// count the number of new CL states required for creating the
// C state new_history from old_history.
// visited_cl_states keeps track of the involved states in CL.
int ComposedStatePredictor::NumNewStates(
    const PhoneContext &old_history, const PhoneContext &new_history,
    std::set<StateId> *visited_cl_states) {
  if (old_history.IsEqual(new_history)) return 0;
  int new_states = 0;
  StateId new_state_id = GetCState(new_history, true);
  StateId old_state_id = GetCState(old_history);
  if (!HasClState(old_state_id)) return 0;
  StateTable::Iterator cl_iter = GetClStates(old_state_id);
  vector<StateId> to_visit;
  Copy(&cl_iter, std::back_inserter(to_visit));
  for (vector<StateId>::const_iterator ci = to_visit.begin(); ci != to_visit.end(); ++ci) {
    StateId cl_state = *ci;
    visited_cl_states->insert(cl_state);
    if (IsReachableState(cl_state, new_state_id, new_history)) {
      ++new_states;
      AddClState(new_state_id, cl_state);
    }
  }
  return new_states;
}

// check whether the new state in C (new_history) creates a new state in CL
// substituting the (old) cl_state.
// check for all predecessor states of cl_state if their C state is
// a valid predecessor of new_history.
bool ComposedStatePredictor::IsReachableState(
    StateId cl_state, StateId new_cstate,
    const PhoneContext &new_history) const {
  const PredecessorList &predecessors = GetPredecessors(cl_state);
  bool valid_predecessors = false;
  for (PredecessorList::const_iterator p = predecessors.begin();
      p != predecessors.end(); ++p) {
    if (!split_clstates_.count(p->first)) {
      if (IsValidStateSequence(p->first, p->second, new_history, p->first == cl_state)) {
        valid_predecessors = true;
        break;
      }
    } else {
      pair<SplitMap::const_iterator, SplitMap::const_iterator> splits =
          split_clstates_.equal_range(p->first);
      for (SplitMap::const_iterator s = splits.first; s != splits.second; ++s) {
        if (IsValidStateSequence(s->second, p->second, new_history, s->second == cl_state)) {
          valid_predecessors = true;
          break;
        }
      }
      if (valid_predecessors) break;
    }
  }
  return valid_predecessors;
}

// check if a C state with history new_history is reachable from
// the given CL state cl_state with arcs labeled with one of the phones
// in labels.
bool ComposedStatePredictor::IsValidStateSequence(
    StateId cl_state, const ContextSet &labels,
    const PhoneContext &new_history, bool is_loop) const {
  const PhoneContext &pre_history = GetHistoryFromCl(cl_state);
  bool is_valid = true;
  if (cl_.center_sets_) {
    ContextSet intersection = new_history.GetContext(0);
    intersection.Intersect(labels);
    is_valid = !intersection.IsEmpty();
  }
  // validity of center set is checked beforehand
  //  => set have_center_set = false
  if (is_valid) {
    is_valid = StateSplitter::IsValidStateSequence(
        pre_history, 0, new_history, false, cl_.num_left_contexts_);
    if (!is_valid && is_loop)
      is_valid = StateSplitter::IsValidStateSequence(
          new_history, 0, new_history, false, cl_.num_left_contexts_);
  }
  return is_valid;
}

// get the C state id for the given history. if no such state exists
// and add == true, a virtual C state is created.
ComposedStatePredictor::StateId ComposedStatePredictor::GetCState(
  const PhoneContext &history, bool add) {
  StateId id = fst::kNoStateId;
  StateMap::const_iterator vcs = vc_states_.find(history);
  if (vcs != vc_states_.end())
    return vcs->second;
  const State *state = cl_.c_->GetState(history);
  if (state) {
    id = cl_.cfst_->GetState(state);
  } else if (add) {
    id = vc_state_history_.size() + vc_state_id_offset_;
    vc_state_history_.push_back(history);
    vc_states_.insert(StateMap::value_type(history, id));
  }
  DCHECK_NE(id, fst::kNoStateId);
  return id;
}

// return true if a CL state with the given C state exists
// (either real or virtual)
bool ComposedStatePredictor::HasClState(StateId c_state) const {
  return cl_.composed_states_->HasFirstState(c_state) ||
         vcl_states_.HasFirstState(c_state);
}

// return an iterator for all CL states consisting of the given
// (real or virtual C state)
ComposedStatePredictor::StateTable::Iterator
ComposedStatePredictor::GetClStates(StateId c_state) const {
  if (vcl_states_.HasFirstState(c_state)) {
    DCHECK(!cl_.composed_states_->HasFirstState(c_state));
    return vcl_states_.TupleIdsForFirstState(c_state);
  } else {
    return cl_.composed_states_->TupleIdsForFirstState(c_state);
  }
}

// add a virtual CL state
void ComposedStatePredictor::AddClState(StateId cstate, StateId old_clstate) {
  StateId lstate = old_clstate <= cl_.composed_states_->MaxId() ?
                   cl_.composed_states_->Tuple(old_clstate).state_id2 :
                   vcl_states_.Tuple(old_clstate).state_id2;
  StateId vcl = vcl_states_.FindState(cstate, lstate);
  StateId origin = old_clstate;
  map<StateId, StateId>::const_iterator oi;
  split_clstates_.insert(SplitMap::value_type(origin, vcl));
  while ((oi = vcl_state_origin_.find(origin)) != vcl_state_origin_.end()) {
    origin = oi->second;
    split_clstates_.insert(SplitMap::value_type(origin, vcl));
  }
  vcl_state_origin_[vcl] = origin;
}

// return the CL predecessor states of the given (real or virtual)
// CL state.
const ComposedStatePredictor::PredecessorList&
ComposedStatePredictor::GetPredecessors(StateId cl_state) const {
  map<StateId, StateId>::const_iterator oi = vcl_state_origin_.find(cl_state);
  if (oi != vcl_state_origin_.end())
    cl_state = oi->second;
  return cl_.cl_predecessors_[cl_state];
}

// return the history of the given (real or virtual) C state.
const PhoneContext& ComposedStatePredictor::GetHistoryFromC(
    StateId c_state) const {
  if (c_state < vc_state_id_offset_) {
    const State *state = cl_.cfst_->GetStateById(c_state);
    DCHECK(state);
    return state->history();
  } else {
    return vc_state_history_[c_state - vc_state_id_offset_];
  }
}

// return the history of the (real or virtual) C state incorporated
// in the given (real or virtual) CL state
const PhoneContext& ComposedStatePredictor::GetHistoryFromCl(
    StateId cl_state) const {
  StateId cstate = cl_state <= cl_.composed_states_->MaxId() ?
                   cl_.composed_states_->Tuple(cl_state).state_id1 :
                   vcl_states_.Tuple(cl_state).state_id1;
  return GetHistoryFromC(cstate);
}

}  // namespace trainc
