// split_predictor.h
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
// prediction for number of required states

#ifndef SPLIT_PREDICTOR_H_
#define SPLIT_PREDICTOR_H_

#include <ext/hash_set>
#include "composed_transducer.h"
#include "context_set.h"

using std::vector;
using std::map;
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_multimap;
using __gnu_cxx::hash_set;

namespace trainc {

class State;

// Interface for split prediction classes.
class AbstractSplitPredictor {
public:
  // maps updated states to originated states
  struct StateUpdate {
    PhoneContext original;
    pair<PhoneContext, PhoneContext> new_states;
    pair<bool, bool> valid_states;
    StateUpdate(const PhoneContext &o, const pair<PhoneContext, PhoneContext> &n,
        const pair<bool, bool> &v) :
      original(o), new_states(n), valid_states(v) {}
  };
  typedef vector<StateUpdate> StateUpdates;

  virtual ~AbstractSplitPredictor() {}
  virtual AbstractSplitPredictor* Clone() const = 0;
  virtual bool IsThreadSafe() const { return true; }

  // Calculate the number of new states required to distinguish between the
  // contexts of a split state model.
  // The split is described by the position context_pos and the context phone
  // partitioning question.
  // models is the list of AllophoneModels involved in the split.
  // the computation can be aborted if more than max_new_states are required
  virtual int Count(int context_pos, const ContextQuestion &question,
                    const AllophoneStateModel::AllophoneRefList &models,
                    int max_new_states) = 0;

  // Returns true if counting is required for the given context position.
  virtual bool NeedCount(int context_pos) const = 0;

  // If discard is true, splits of models not present in the transducer
  // result in a state count of the given count value.
  virtual void SetDiscardAbsentModels(bool discard) {}

  // Initialize caches. Must be called before calls to Count(),
  // after changing the tranducer
  virtual void Init() {}

  static const int kInvalidCount;
};


// calculates the number of new states required by
// a model split for a given transducer.
class SplitPredictor : public AbstractSplitPredictor {
  typedef hash_set<State*, StatePtrHash> StateRefSet;
 public:
  explicit SplitPredictor(const ConstructionalTransducer &t)
      : transducer_(t), center_set_(t.HasCenterSets()) {}
  virtual ~SplitPredictor() {}

  SplitPredictor* Clone() const { return new SplitPredictor(transducer_); }

  // Count the number of new states required by splitting states
  // at position context_pos using the partition question.
  virtual int Count(int context_pos, const ContextQuestion &question,
                    const AllophoneStateModel::AllophoneRefList &models,
                    int max_new_states=0) {
    return Count(context_pos, question, models, max_new_states, NULL);
  }
  int Count(int context_pos, const ContextQuestion &question,
            const AllophoneStateModel::AllophoneRefList &models,
            int max_new_states, StateUpdates *updates);

  virtual bool NeedCount(int context_pos) const {
    return context_pos != 1;
  }

 private:
  typedef Hash<PhoneContext> PhoneContextHash;
  typedef Equal<PhoneContext> PhoneContextEqual;

  typedef hash_set<PhoneContext, PhoneContextHash, PhoneContextEqual>
      HistorySet;
  void GetStates(int context_pos, const ContextQuestion &question,
                 const AllophoneStateModel::AllophoneRefList &models,
                 StateRefSet *states) const;
  void GetPredecessors(int context_pos, const StateRefSet &states);
  void GetHistories(const StateRefSet &states, HistorySet *histories) const;
  void UpdateSuccessors(int position, const PhoneContext &state,
                        const pair<PhoneContext, PhoneContext> &new_histories,
                        const pair<bool, bool> &valid_states);
  void Reset();
  const ConstructionalTransducer &transducer_;
  bool center_set_;
  vector<HistorySet> closure_;
  DISALLOW_COPY_AND_ASSIGN(SplitPredictor);
};

class ComposedStatePredictor : public AbstractSplitPredictor {
  typedef ComposedTransducer::StateTable StateTable;
public:
  ComposedStatePredictor(const ComposedTransducer &cl);
  virtual ~ComposedStatePredictor();

  ComposedStatePredictor* Clone() const {
    return new ComposedStatePredictor(cl_);
  }

  virtual int Count(int context_pos, const ContextQuestion &question,
                    const AllophoneStateModel::AllophoneRefList &models,
                    int max_new_states);

  virtual bool NeedCount(int context_pos) const {
    return context_pos != 1;
  }

private:
  typedef ComposedTransducer::StateId StateId;
  typedef ComposedTransducer::PredecessorList PredecessorList;
  typedef hash_map<PhoneContext, StateId,
                   Hash<PhoneContext>, Equal<PhoneContext> > StateMap;
  typedef hash_multimap<StateId, StateId> SplitMap;

  void Reset();
  bool IsReachableState(StateId cl_state, StateId new_cstate,
                        const PhoneContext &new_history) const;
  int NumNewStates(const PhoneContext &old_history,
                   const PhoneContext &new_history,
                   std::set<StateId> *visited_cl_states);
  StateId GetCState(const PhoneContext &history, bool add = false);
  const PhoneContext& GetHistoryFromC(StateId c_state) const;
  const PhoneContext& GetHistoryFromCl(StateId cl_state) const;
  StateTable::Iterator GetClStates(const StateId c_state) const;
  bool HasClState(const StateId c_state) const;
  void AddClState(StateId cstate, StateId old_clstate);
  const PredecessorList& GetPredecessors(StateId cl_state) const;
  bool IsValidStateSequence(StateId cl_state, const ContextSet &labels,
                            const PhoneContext &new_history, bool is_loop) const;


  const ComposedTransducer &cl_;
  SplitPredictor *count_c_;
  StateMap vc_states_;
  vector<PhoneContext> vc_state_history_;
  SplitMap split_clstates_;
  int vc_state_id_offset_;
  StateTable vcl_states_;
  map<StateId, StateId> vcl_state_origin_;
  DISALLOW_COPY_AND_ASSIGN(ComposedStatePredictor);
};

}

#endif /* SPLIT_PREDICTOR_H_ */
