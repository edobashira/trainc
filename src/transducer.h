// transducer.h
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
// Copyright 2010 Google Inc. All Rights Reserved.
// Author: rybach@google.com (David Rybach)
//
// Definition of the intermediate transducer used during the
// construction of the context dependency transducer.
//
// A state in the context dependency transducer is defined by
// a set of center phones and a sequence of phone sets for the history
// of phones read so far.
// An arc in the transducer has an AllophoneModel m as input label and
// a phone p as output label:
// For context dependent phones with 2 left contexts and 1 right context:
// state q_1 = (H'_1, H_1, c_1) with history H'_1, H_1 and center phone c_1
// state q_2 = (H'_2, H_2, c_2)
// arc: (q_1, m, p, q_2) with
//    p == c_2
//    c_1 \in H_2
//    H_1 \subset H'_2
//
// A HMM state model split induces the following splits in the C
// transducer:
//
// Split HMM state model m into m_1, m_2 using context position pos
// and a context partition p_1 / p_2:
//
// H = all HMM models h which share m
// foreach h in H:
//   h_1, h_2 = new HMM models for m_1, m_2
//   A(h) = arcs with input label h
//   P(h) = states with outgoing arc in A(h)
//   if pos == 1:  // split on right context
//     foreach q in P(h):
//       relabel outgoing arcs with h_1 / h_2 depending on the output label
//          of the arc
//   else:
//     if pos < -1:
//       recursively split predecessor states of all q in P(h) at pos + 1
//     foreach q in P(h):
//       split q in q_1 / q_2 by intersecting its context history with
//           p_1 / p_2
//       redirect incoming arcs from q to q_1 / q_2 based on the output symbol
//           of the arc in the predecessor states's history
//       create outgoing for q_1, q_2 based on the outgoing arcs of q and
//           the new models m_1 / m_2

#ifndef TRANSDUCER_H_
#define TRANSDUCER_H_

#include <ext/hash_map>
#include <ext/hash_set>
#include <list>
#include <map>
#include <set>
#include <ext/slist>
#include <utility>
#include <vector>
#include "context_set.h"
#include "hash.h"
#include "phone_models.h"
#include "util.h"

using std::list;
using std::map;
using std::set;
using std::vector;
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;
using __gnu_cxx::slist;


namespace trainc {

class State;
class StateIterator;
class ArcIterator;

// Arc in the intermediate transducer.
// An Arc has a AllophoneModel as input and a phone as output.
// Differs from arcs in nlp_fst, because it holds a pointer to
// both its source and target states and it doesn't carry a weight.
class Arc {
 public:
  Arc(State *source, State *target, const AllophoneModel *input, int output)
      : source_(source), target_(target), input_(input), output_(output) {}

  State* source() const { return source_; }
  State* target() const { return target_; }
  const AllophoneModel* input() const { return input_; }
  void SetInput(const AllophoneModel *input) { input_ = input; }
  void SetTarget(State *target) { target_ = target; }
  int output() const { return output_; }

 private:
  State *source_, *target_;
  const AllophoneModel *input_;
  int output_;
};

typedef ListIteratorHash<Arc> ArcRefHash;
typedef ListIteratorCompare<Arc> ArcRefCompare;

class State;
typedef PointerHash<State> StatePtrHash;


// State in the intermediate transducer.
// A state consists of the center phone, i.e. the most recently read
// phone, and the context sets of the phone history.
// Arcs leaving this state are stored in a list, because arcs will be
// frequently added and removed.
// A state keeps tracks of incoming arcs, i.e. arcs with
//   arc.target == this
class State {
 public:
  typedef list<Arc> ArcList;
  typedef ArcList::iterator ArcRef;
  typedef hash_set<ArcRef, ArcRefHash > ArcRefList;
  typedef hash_set<State*, StatePtrHash> StateRefSet;


  // Initialize state with the given center phone and history.
  // num_phones is the total number of phones. The total number
  // of phones is required for the size of Arc vector. All states
  // have an outgoing arc for each phone.
  explicit State(const PhoneContext &history);
  virtual ~State();

  // Access to a set at the given context position.
  const ContextSet& GetHistory(int position) const {
    return history_.GetContext(position);
  }

  // Accessor for the history of the state.
  const PhoneContext& history() const {
    return history_;
  }

  // The center phone or phones.
  const ContextSet& center() const { return GetHistory(0); }

  // Add an arc with this state as source.
  ArcRef AddArc(const AllophoneModel *input, int output, State *target);

  // Remove the given arc.
  void RemoveArc(ArcRef arc);

  // Remove all arcs.
  void ClearArcs();

  // Access to the list of arcs.
  const ArcList& GetArcs() const {
    return arcs_;
  }

  // Mutable access to the list of arcs.
  ArcList* GetArcsRef() {
    return &arcs_;
  }

  // Register an incoming arc.
  void AddIncomingArc(ArcRef arc);

  // Remove an incoming arc.
  void RemoveIncomingArc(ArcRef arc);

  // Access to the list of incoming arcs.
  const ArcRefList& GetIncomingArcs() const {
    return incoming_arcs_;
  }

  // Mutable access to the list of incoming arcs.
  ArcRefList* GetIncomingArcsRef() {
    return &incoming_arcs_;
  }

  // Set of states having an arc to this state.
  const StateRefSet& GetPredecessorStates() const;

 private:
  friend class ArcIterator;
  // ContextSet center_;
  PhoneContext history_;
  ArcList arcs_;
  ArcRefList incoming_arcs_;
  // cache predecessor states
  class PredecessorCache;
  friend class PredecessorCache;
  mutable PredecessorCache *predecessors_;
};

class StateSplitter;
class AbstractSplitPredictor;
class SplitPredictor;

// Interface for transducer classes which allow for state counts and
// prediction of required new states.
class StateCountingTransducer {
public:
  StateCountingTransducer() {}
  virtual ~StateCountingTransducer() {}

  virtual int NumStates() const = 0;

  // Create a SplitPredictor that calculates the number of new states
  // required for a model split.
  virtual AbstractSplitPredictor* CreateSplitPredictor() const = 0;

  virtual void ApplyModelSplit(int context_pos, const ContextQuestion *question,
                               AllophoneModel *old_model, int hmm_state,
                               const AllophoneModel::SplitResult &new_models)
                               = 0;

  virtual void FinishSplit() {}
};

// Interface for observer classes
// receiving structure change events of ConstructionalTransducer.
class TransducerChangeObserver {
public:
  virtual ~TransducerChangeObserver() {}
  virtual void NotifyAddState(const State *) {}
  virtual void NotifyRemoveState(const State *) {}
  virtual void NotifyAddArc(const State::ArcRef) {}
  virtual void NotifyRemoveArc(const State::ArcRef) {}
};

// Transducer created during the construction of the
// context dependency transducer.
// nlp_fst::Fst is not used here because:
//   * transducer operations are not required
//   * frequent deletion and insertion of states and arcs, which
//     is not compatible with using vector as container of
//     states and arcs
//   * need pointer or index to arcs and states which stay valid
//     when other arcs/states are added/removed.
//
// The set of states is organized in an array of hash_maps, one map for each
// phone, in order to find already existing states for a (phone, history) tuple.
// The ConstructionTransducer maintains a mapping from AllophoneModels to
// arcs to find the arcs having a specific AllophoneModel as input.
class ConstructionalTransducer : public StateCountingTransducer {
 public:
  // Setup the internal data structures for a transducer
  // with the given number of phone symbols in the output alphabet.
  // If center_set = true, states can represent several center phones.
  ConstructionalTransducer(int num_phones,
                           int num_left_contexts, int num_right_contexts,
                           bool center_set = false);


  // Deletes all remaining State objects that have been created by AddState().
  virtual ~ConstructionalTransducer();

  // Create an empty ConstructionalTransducer with the same properties.
  ConstructionalTransducer* Clone() const;

  // Return the state matching the given description
  // or NULL if no such state exists.
  // Returns only the first state if unique_history == false and
  // more than one state exist for the given description.
  State* GetState(const PhoneContext &history) const;

  // Create a new state with the given phone and phone history.
  // Ownership of the State object remains at this object.
  State* AddState(const PhoneContext &history);

  // Delete the given state.
  // The pointer will be invalid after this call.
  void RemoveState(const State* state);

  // Add an arc to the transducer.
  State::ArcRef AddArc(State *source, State *target,
                       const AllophoneModel *input, int output);

  // Delete the given arc.
  // The iterator will be invalid after this call.
  void RemoveArc(State::ArcRef arc);

  // Change the input of the given arc.
  void UpdateArcInput(State::ArcRef arc, const AllophoneModel *new_input);

  // Notify the transducer that the given AllophoneModel is no longer used.
  void RemoveModel(const AllophoneModel *model);

  // Performs the state splitting and arc relabeling required to distinguish
  // between the the split models.
  // The split has been performed on old_model and resulted in the two models
  // in new_models. The context at position context_pos was used. The
  // AllophoneStateModel split is at the HMM state hmm_state.
  void ApplyModelSplit(int context_pos, const ContextQuestion *question,
                       AllophoneModel *old_model, int hmm_state,
                       const AllophoneModel::SplitResult &new_models);

  // Number of states in the transducer.
  int NumStates() const;

  // Return true if the center phones of a state are modeled by sets and
  // are therefore "splitable".
  bool HasCenterSets() const {
    return center_set_;
  }

  int NumLeftContexts() const { return num_left_contexts_; }
  int NumRightContexts() const { return num_right_contexts_; }
  int NumPhones() const { return num_phones_; }

  // Collect predecessor states of the set of given states.
  void GetPredecessorStatesOfSet(const State::StateRefSet &states,
                                 State::StateRefSet *predecessors) const;

  // Collect all arcs having the given model as input label.
  void GetArcsWithModel(const AllophoneModel *model,
                        vector<State::ArcRef> *arcs) const;

  // Collect all states which have an arc having the given model
  // as input label.
  void GetStatesForModel(const AllophoneModel *model,
                         State::StateRefSet *states) const;

  // can't use a covariant return type here, because the inheritance
  // relation between AbstractSplitPredictor and SplitPredictor is not known.
  // split_predictor.h can't be included because of circular dependencies.
  AbstractSplitPredictor* CreateSplitPredictor() const;

  // Register an observer object.
  // Ownership remains at caller.
  void RegisterObserver(TransducerChangeObserver *observer) {
    observer_ = observer;
  }

 private:
  friend class StateIterator;

  typedef Hash<PhoneContext> PhoneContextHash;
  typedef Equal<PhoneContext> PhoneContextEqual;
  typedef hash_map<PhoneContext, State*, PhoneContextHash, PhoneContextEqual>
      StateHashMap;
  // typedef hash_set<State::ArcRef, ArcRefHash> ArcRefSet;
  typedef set<State::ArcRef, ListIteratorCompare<Arc> > ArcRefSet;
  typedef hash_map<const AllophoneModel*, ArcRefSet, PointerHash<const AllophoneModel> > ModelToArcMap;
  typedef State::StateRefSet StateRefSet;

  void SetModelToArc(State::ArcRef arc, const AllophoneModel *model);
  void RemoveModelToArc(State::ArcRef arc, const AllophoneModel *model);

  int num_phones_;
  int num_left_contexts_, num_right_contexts_;
  bool center_set_;
  // history to state mapping and state container
  StateHashMap state_map_;
  // list of arcs that have a specific AllophoneModel as input
  ModelToArcMap arcs_with_model_;
  int num_states_;
  StateSplitter *splitter_;
  TransducerChangeObserver *observer_;
  DISALLOW_COPY_AND_ASSIGN(ConstructionalTransducer);
};

// Iterator for the states of a ConstructionalTransducer.
class StateIterator {
 public:
  explicit StateIterator(const ConstructionalTransducer &t)
      : iter_(t.state_map_.begin()), end_(t.state_map_.end()) {}

  bool Done() const {
    return iter_ == end_;
  }

  void Next() {
    ++iter_;
  }

  const State& Value() const {
    return *iter_->second;
  }

 private:
  ConstructionalTransducer::StateHashMap::const_iterator iter_, end_;
};


// Iterator for the arcs of a state in a ConstructionalTransducer.
class ArcIterator {
 public:
  explicit ArcIterator(const State &s)
      : iter_(s.arcs_.begin()), end_(s.arcs_.end()) {}

  bool Done() const {
    return iter_ == end_;
  }

  void Next() {
    ++iter_;
  }

  const Arc& Value() const {
    return *iter_;
  }

 private:
  State::ArcList::const_iterator iter_, end_;
};

}  // namespace trainc

#endif  // TRANSDUCER_H_
