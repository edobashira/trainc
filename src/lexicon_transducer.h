// lexicon_transducer.h
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
// Transducer with 3 labels (input CI phone, output label, AllophoneModel)
// intended for model splitting

#ifndef LEXICON_TRANSDUCER_H_
#define LEXICON_TRANSDUCER_H_

#include <list>
#include <set>
#include <stack>
#include <vector>
#include <ext/hash_map>
#include <ext/hash_set>
#include "fst/arc.h"
#include "fst/expanded-fst.h"
#include "fst/fst.h"
#include "fst/vector-fst.h"
#include "transducer.h"

using std::list;
using std::set;
using std::vector;
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;

namespace trainc {

class LexiconStateSplitter;

// Arc in a LexiconTransducer.
// In addition to an fst::StdArc, it contains a AllophoneModel* and
// the state id of its predecessor state.
class LexiconArc : public fst::StdArc {
public:
  typedef fst::StdArc::StateId StateId;
  typedef fst::StdArc::Weight Weight;
  typedef fst::StdArc::Label Label;

  LexiconArc(Label i, Label o, const Weight &w, StateId n)
      : fst::StdArc(i, o, w, n), model(NULL), prevstate(fst::kNoStateId) {
    // prevstate is set in LexiconTransducer::AddArc()
  }


  LexiconArc(Label i, Label o, const AllophoneModel *m, const Weight &w,
             StateId n)
      : fst::StdArc(i, o, w, n), model(m), prevstate(fst::kNoStateId) {}

  static const string Type() {
    static const string type = "lexicon";
    return type;
  }

  const AllophoneModel *model;
  StateId prevstate;

private:
  template<bool reverse>
  struct ProxyTpl {
    typedef LexiconArc::StateId StateId;
    typedef LexiconArc Arc;
    static StateId TargetState(const Arc &arc) {
      return reverse ? arc.prevstate : arc.nextstate;
    }
    static void SetTargetState(Arc *arc, StateId s) {
      StateId &t = reverse ? arc->prevstate : arc->nextstate;
      t = s;
    }
    static StateId SourceState(const Arc &arc) {
      return reverse ? arc.nextstate : arc.prevstate;
    }
    static void SetSourceState(Arc *arc, StateId s) {
      StateId &t = reverse ? arc->nextstate : arc->prevstate;
      t = s;
    }
  };
public:
  typedef ProxyTpl<false> Proxy;
  typedef ProxyTpl<true> ReverseProxy;
};

class LexiconArcIterator;

// State in a LexiconTransducer.
// Stores arcs in a list, because arcs get removed and inserted frequently.
// Keeps references to incoming arcs.
// The state holds context sets for incoming and outgoing arcs which represent
// the set of of input labels (context_).
class LexiconState {
public:
  typedef LexiconArc Arc;
  typedef Arc::Weight Weight;
  typedef Arc::StateId StateId;
  typedef list<Arc> ArcList;
  typedef ArcList::iterator ArcRef;
  typedef hash_set<ArcRef, ListIteratorHash<Arc> > ArcRefSet;

  explicit LexiconState(const PhoneContext &context)
      : final(Weight::Zero()), context_(context),
        n_out_eps_(0), n_in_eps_(0) {}

  ArcRef AddArc(const Arc &arc) {
    arcs_.push_front(arc);
    if (!arc.model) ++n_out_eps_;
    return arcs_.begin();
  }

  ArcRef RemoveArc(ArcRef arc) {
    if (!arc->model) --n_out_eps_;
    return arcs_.erase(arc);
  }

  void Clear() {
    arcs_.clear();
    n_out_eps_ = 0;
  }

  size_t NumArcs() const {
    return arcs_.size();
  }

  size_t NumOutputEpsilons() const;

  size_t NumInputEpsilons() const {
    return n_out_eps_;
  }

  size_t NumIncomingEpsilons() const {
    return n_in_eps_;
  }

  void AddIncoming(ArcRef arc) {
    if (!arc->model) ++n_in_eps_;
    incoming_arcs_.insert(arc);
  }

  void RemoveIncoming(ArcRef arc) {
    if (!arc->model) --n_in_eps_;
    incoming_arcs_.erase(arc);
  }

  const ArcRefSet& GetIncomingArcs() const {
    return incoming_arcs_;
  }

  const ArcList& Arcs() const {
    return arcs_;
  }

  ArcList& Arcs() {
    return arcs_;
  }

  PhoneContext* ContextRef() {
    return &context_;
  }

  const PhoneContext& Context() const {
    return context_;
  }

  const ContextSet& GetContext(int pos) const {
    return context_.GetContext(pos);
  }

  void UpdateContext();

  Weight final;

  class BackwardArcIterator;
  class ForwardArcIterator;
  class ConstForwardArcIterator;
protected:
  ArcList arcs_;
  ArcRefSet incoming_arcs_;
  PhoneContext context_; // , max_context_;
  size_t n_out_eps_, n_in_eps_;
};

// Iterator over incoming arcs, arcs are reversed.
class LexiconState::BackwardArcIterator {
  typedef LexiconState::ArcRefSet ArcRefSet;
  typedef LexiconState::ArcRef ArcRef;
  typedef LexiconArc Arc;
public:
  typedef Arc::ReverseProxy ArcAccess;
  BackwardArcIterator(const LexiconState *state)
      : arcs_(state->GetIncomingArcs()), i_(arcs_.begin()) {}
  bool Done() const { return i_ == arcs_.end(); }
  void Next() { ++i_; }
  const Arc& Value() const { return **i_; }
  ArcRef Ref() const { return *i_; }
protected:
  const ArcRefSet &arcs_;
  ArcRefSet::const_iterator i_;
};

// Iterator over outgoing arcs.
class LexiconState::ForwardArcIterator {
  typedef LexiconArc Arc;
  typedef LexiconState::ArcList ArcList;
  typedef LexiconState::ArcRef ArcRef;
public:
  typedef Arc::Proxy ArcAccess;
  ForwardArcIterator(LexiconState *state)
      : arcs_(state->Arcs()), i_(arcs_.begin()) {}
  bool Done() const { return i_ == arcs_.end(); }
  void Next() { ++i_; }
  const Arc& Value() const { return *i_; }
  ArcRef Ref() const { return i_; }
protected:
  ArcList &arcs_;
  ArcRef i_;
};

// Iterator over const outgoing arcs.
class LexiconState::ConstForwardArcIterator {
  typedef LexiconArc Arc;
  typedef LexiconState::ArcList ArcList;
  typedef LexiconState::ArcRef ArcRef;
public:
  typedef Arc::Proxy ArcAccess;
  ConstForwardArcIterator(const LexiconState *state)
      : arcs_(state->Arcs()), i_(arcs_.begin()) {}
  bool Done() const { return i_ == arcs_.end(); }
  void Next() { ++i_; }
  const Arc& Value() const { return *i_; }
protected:
  const ArcList &arcs_;
  ArcList::const_iterator i_;
};

class LexiconArcIterator;

// fst-related implementation of the LexiconTransducer
// States are created on the heap, with pointers stored in a vector.
// The states cannot be stored in the vector directly, because if the
// vector is resized, the State::ArcRefs will get invalid.
// Deleted states are re-used after calling PurgeStates(). Deleted states
// are not immediately re-used, because LexiconStateSplitter requires unique
// StateIds during processing of a model split.
//
// TODO(rybach): share more code with ConstructionalTransducer.
class LexiconTransducerImpl : public fst::FstImpl<LexiconArc> {
public:
  typedef LexiconArc Arc;
  typedef LexiconState State;
  typedef LexiconArc::Weight Weight;
  typedef State::StateId StateId;
  typedef State::ArcRef ArcRef;
  typedef hash_set<ArcRef, ListIteratorHash<Arc> > ArcRefSet;
  typedef hash_map<const AllophoneModel*, ArcRefSet,
      PointerHash<const AllophoneModel> > ModelToArcMap;


  LexiconTransducerImpl();
  StateId Start() const {
    DCHECK(!start_.empty());
    return *start_.begin();
  }
  Weight Final(StateId s) const {
    return states_[s] ? states_[s]->final : Weight::Zero();
  }
  StateId NumStates() const {
    return states_.size() - (deleted_states_.size() + free_states_.size());
  }
  size_t NumArcs(StateId s) const {
    return states_[s] ? states_[s]->NumArcs() : 0;
  }
  size_t NumInputEpsilons(StateId s) const {
    return states_[s] ? states_[s]->NumInputEpsilons() : 0;
  }
  size_t NumOutputEpsilons(StateId s) const {
    return states_[s] ? states_[s]->NumOutputEpsilons() : 0;
  }
  void SetFinal(StateId s, const Weight &w) {
    if (states_[s]) states_[s]->final = w;
  }
  void InitStateIterator(fst::StateIteratorData<Arc> *) const;
  void InitArcIterator(StateId s, fst::ArcIteratorData<Arc> *) const;
  bool Write(std::ostream &, const fst::FstWriteOptions &) const {
    return false;
  }

  State* GetStateRef(StateId s) {
    DCHECK_LT(s, states_.size());
    return states_[s];
  }
  const State* GetState(StateId s) const {
    DCHECK_LT(s, states_.size());
    return states_[s];
  }
  void SetStart(StateId s) { start_.insert(s); }
  StateId AddState(const PhoneContext &c);
  void RemoveState(StateId s);
  ArcRef AddArc(StateId s, const Arc &arc);
  ArcRef RemoveArc(StateId s, State::ArcRef arc);
  void UpdateArc(State::ArcRef arc, const AllophoneModel *new_model);
  bool IsStart(StateId s) const {
    return start_.count(s);
  }

  void PurgeStates();

  const ModelToArcMap& ModelToArcs() const { return arcs_with_model_; }
protected:
  void SetModelToArc(ArcRef arc, const AllophoneModel *m);
  void RemoveModelToArc(ArcRef arc, const AllophoneModel *m);

  std::vector<State*> states_;
  std::vector<StateId> free_states_, deleted_states_;
  std::set<StateId> start_;
  ModelToArcMap arcs_with_model_;
};

class LexiconStateSiblings;
class EpsilonClosure;
class StateContexts;

// An CD HMM to word transducer (e.g. CL or CLG) intended for model splitting.
// LexiconTransducer forwards all calls to ApplyModelSplit and FinishSplit
// to a C transducer if set beforehand with SetCTransducer().
class LexiconTransducer : public fst::ImplToExpandedFst<LexiconTransducerImpl>,
    public StateCountingTransducer {
public:
  typedef LexiconArc Arc;
  typedef LexiconState State;
  typedef State::StateId StateId;
  typedef State::ArcRef ArcRef;
  typedef fst::ImplToExpandedFst<LexiconTransducerImpl> Impl;

  LexiconTransducer()
      : Impl(new LexiconTransducerImpl()), num_phones_(0), det_split_(true),
          shifted_(true), empty_context_(0, 0, 0), c_(NULL), splitter_(NULL),
          siblings_(NULL), empty_model_(new AllophoneModel(0, 0)) {
    contexts_[0] = contexts_[1] = NULL;
    closure_[0] = closure_[1] = NULL;
  }

  explicit LexiconTransducer(const LexiconTransducer &o);
  virtual ~LexiconTransducer();

  LexiconTransducer* Copy(bool safe = false) const {
    return new LexiconTransducer(*this);
  }

  // StateCountingTransducer interface
  int NumStates() const { return GetImpl()->NumStates(); }

  AbstractSplitPredictor* CreateSplitPredictor() const;

  void ApplyModelSplit(int context_pos, const ContextQuestion *question,
                       AllophoneModel *old_model, int hmm_state,
                       const AllophoneModel::SplitResult &new_models);

  void FinishSplit();

  // Fst interface
  void InitStateIterator(fst::StateIteratorData<Arc> *data) const {
    GetImpl()->InitStateIterator(data);
  }

  void InitArcIterator(StateId s, fst::ArcIteratorData<Arc> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

  void SetFinal(StateId s, const Weight &w) { GetImpl()->SetFinal(s, w); }

  // other methods

  const State* GetState(StateId s) const { return GetImpl()->GetState(s); }
  State* GetStateRef(StateId s) const { return GetImpl()->GetStateRef(s); }
  void SetStart(StateId s) { GetImpl()->SetStart(s); }
  bool IsStart(StateId s) const { return GetImpl()->IsStart(s); }
  StateId AddState() {
    return GetImpl()->AddState(empty_context_);
  }
  void RemoveState(StateId s) { GetImpl()->RemoveState(s); }
  ArcRef AddArc(StateId s, const Arc &arc) { return GetImpl()->AddArc(s, arc); }
  void RemoveArc(StateId s, State::ArcRef arc) { GetImpl()->RemoveArc(s, arc); }
  void UpdateArc(State::ArcRef arc, const AllophoneModel *new_model) {
    GetImpl()->UpdateArc(arc, new_model);
  }
  PhoneContext* ContextRef(StateId s) {
    return GetImpl()->GetStateRef(s)->ContextRef();
  }
  const PhoneContext Context(StateId s) const {
    return GetImpl()->GetState(s)->Context();
  }

  void SetContextSize(int num_phones, int num_left_contexts,
                      int num_right_contexts, bool center_set);

  void SetCTransducer(ConstructionalTransducer *c);

  void Init(const fst::StdExpandedFst &l, const ModelManager &models,
            const map<int, int> phone_mapping, int boundary_phone);

  void GetStatesForModel(const AllophoneModel *model,
                         bool sourceState,
                         vector<StateId> *states,
                         bool unique=true) const;

  void GetArcsForModel(const AllophoneModel *model,
                       std::vector<ArcRef> *arcs) const;

  bool HasModel(const AllophoneModel *model) const;

  template<class Iter>
  void FindReachable(StateId start, set<StateId> *states) const;

  void PurgeStates() {
    GetImpl()->PurgeStates();
  }

  LexiconStateSiblings* GetSiblings() const {
    return siblings_;
  }

  const StateContexts* GetStateContexts(int pos) const {
    return contexts_[pos];
  }

  EpsilonClosure* GetEpsilonClosure(int pos) const {
    return closure_[pos];
  }

  void ResetContexts(int pos);

  int NumPhones() const {
    return num_phones_;
  }

  void SetSplitDerministic(bool det) {
    det_split_ = det;
  }

  bool DeterministicSplit() const {
    return det_split_;
  }

  void SetShifted(bool shifted) {
    shifted_ = shifted;
  }

  bool IsShifted() const {
    return shifted_;
  }
  const AllophoneModel* EmptyModel() const {
    return empty_model_;
  }
  bool IsEmptyModel(const AllophoneModel *model) const {
    return model == empty_model_;
  }
protected:
  class Initializer;
  typedef LexiconTransducerImpl::ModelToArcMap ModelToArcMap;

  int num_phones_;
  bool det_split_, shifted_;
  PhoneContext empty_context_;
  ConstructionalTransducer *c_;
  LexiconStateSplitter *splitter_;
  LexiconStateSiblings *siblings_;
  StateContexts* contexts_[2];
  EpsilonClosure* closure_[2];
  const AllophoneModel *empty_model_;
};

template<class ArcIter>
inline void LexiconTransducer::FindReachable(StateId start,
                                             set<StateId> *states) const {
  typedef typename ArcIter::ArcAccess ArcAccess;
  std::stack<StateId> to_visit;
  to_visit.push(start);
  while (!to_visit.empty()) {
    StateId s = to_visit.top();
    to_visit.pop();
    if (states->count(s)) continue;
    states->insert(s);
    for (ArcIter aiter(GetState(s)); !aiter.Done(); aiter.Next()) {
      StateId ns = ArcAccess::TargetState(aiter.Value());
      if (!aiter.Value().model && !states->count(ns))
        to_visit.push(ns);
    }
  }
  states->erase(start);
}


// ArcIterator for LexiconTransducer with OpenFst interface.
class LexiconArcIterator : public fst::ArcIteratorBase<LexiconArc> {
public:
  LexiconArcIterator(const LexiconTransducer &l, LexiconTransducer::StateId s) {
    if (l.GetState(s)) {
      begin_ = i_ = l.GetState(s)->Arcs().begin();
      end_ = l.GetState(s)->Arcs().end();
    }
  }
  explicit LexiconArcIterator(const LexiconState &s)
      : begin_(s.Arcs().begin()), i_(begin_), end_(s.Arcs().end()) {}
  virtual ~LexiconArcIterator() {}
  bool Done() const { return i_ == end_; }
  const LexiconArc& Value() const { return *i_; }
  void Next() { ++i_; }
  size_t Position() const { return std::distance(begin_, i_); }
  void Reset() { i_ = begin_; }
  void Seek(size_t a) { Reset(); std::advance(i_, a); }
  uint32 Flags() const { return fst::kArcValueFlags;  }
  void SetFlags(uint32 flags, uint32 mask) {}
private:
  virtual bool Done_() const { return Done(); }
  virtual const LexiconArc& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual size_t Position_() const { return Position(); }
  virtual void Reset_() { Reset(); }
  virtual void Seek_(size_t a) { Seek(a); }
  virtual uint32 Flags_() const { return Flags(); }
  virtual void SetFlags_(uint32 flags, uint32 mask) { SetFlags(flags, mask); }
  LexiconState::ArcList::const_iterator begin_, i_, end_;
};

// DEBUG
inline void Convert(const LexiconTransducer &l, fst::StdVectorFst *cl) {
  for (fst::StateIterator<LexiconTransducer> siter(l); !siter.Done();
      siter.Next()) {
    LexiconTransducer::StateId s = siter.Value();
    while (s >= cl->NumStates()) cl->AddState();
    cl->SetFinal(s, l.Final(s));
    for (LexiconArcIterator aiter(l, s); !aiter.Done(); aiter.Next()) {
      const LexiconArc &arc = aiter.Value();
      while (arc.nextstate >= cl->NumStates()) cl->AddState();
      fst::StdArc new_arc;
      new_arc.ilabel = reinterpret_cast<ssize_t>(arc.model); // arc.ilabel;
      new_arc.olabel = arc.ilabel; // arc.olabel
      new_arc.weight = arc.weight;
      new_arc.nextstate = arc.nextstate;
      cl->AddArc(s, new_arc);
    }
  }
  cl->SetStart(l.Start());
}

}  // namespace trainc

#endif  // LEXICON_TRANSDUCER_H_
