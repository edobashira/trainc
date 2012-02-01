// fst_interface.h
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
// Adaptor for ConstructionalTransducer to the OpenFst Fst

#ifndef FST_INTERFACE_H_
#define FST_INTERFACE_H_

#include "fst/fst.h"
#include "fst/expanded-fst.h"
#include "fst/test-properties.h"
#include "transducer.h"

namespace trainc {

// Implementation of the FstInterface
class FstInterfaceImpl : public fst::FstImpl<fst::StdArc> {
public:
  typedef fst::StdFst::Arc Arc;
  typedef Arc::Weight Weight;
  typedef Arc::StateId StateId;

  FstInterfaceImpl()
      : c_(NULL), root_(NULL), boundary_state_(NULL),
        num_states_(0), boundary_phone_(-1) {
    SetType(kType);
    SetProperties(kProperties);
  }
  virtual ~FstInterfaceImpl() {
    delete root_;
  }
  void Init(ConstructionalTransducer *c, int boundary_phone);
  StateId Start() const {
    return kRootId;
  }
  Weight Final(StateId) const;
  size_t NumArcs(StateId) const;
  size_t NumInputEpsilons(StateId) const;
  size_t NumOutputEpsilons(StateId) const { return 0; }
  StateId NumStates() const { return c_->NumStates(); }
  void InitStateIterator(fst::StateIteratorData<Arc> *) const;
  void InitArcIterator(StateId s, fst::ArcIteratorData<Arc> *) const;
  virtual bool Write(std::ostream &strm, const fst::FstWriteOptions &opts) const {
    return false;
  }

  StateId MaxStateId() const;
  StateId AddState(const State *state);
  StateId RemoveState(const State *state);
  StateId GetState(const State *state);
  const State* GetStateById(StateId id) const;
  void UpdateStartState();


  class StateIterator;
  class ArcIterator;
private:
  typedef hash_map<const State*, StateId, PointerHash<State> > StateMap;
  typedef trainc::Arc CArc;
  typedef trainc::StateIterator CStateIterator;
  typedef trainc::ArcIterator CArcIterator;

  StateId GetStateId(const State *state, bool add=true);
  const State* FindBoundaryState() const;

  static const string kType;
  static const uint64 kProperties;
  static const StateId kRootId;;
  const ConstructionalTransducer *c_;
  StateMap state_ids_;
  std::vector<const State*> id2state_;
  State *root_;
  const State *boundary_state_;
  int num_states_;
  int boundary_phone_;
  std::list<StateId> free_ids_;

  friend class StateIterator;
  friend class ArcIterator;
};


// Adaptor for a ConstructionalFst to fst::Fst interface
class FstInterface : public fst::ImplToExpandedFst<FstInterfaceImpl> {
public:
  typedef FstInterfaceImpl Impl;
  typedef Impl::StateId StateId;

  FstInterface() : fst::ImplToExpandedFst<Impl>(new Impl()) {}
  FstInterface(const FstInterface &fst) : fst::ImplToExpandedFst<Impl>(fst) {}

  void Init(ConstructionalTransducer *c, int boundary_phone) {
    GetImpl()->Init(c, boundary_phone);
  }

  FstInterface* Copy(bool safe = false) const {
    return new FstInterface(*this);
  }
  const fst::SymbolTable* InputSymbols() const { return NULL; }
  const fst::SymbolTable* OutputSymbols() const { return NULL; }
  void InitStateIterator(fst::StateIteratorData<Arc> *data) const {
    return GetImpl()->InitStateIterator(data);
  }
  void InitArcIterator(StateId s, fst::ArcIteratorData<Arc> *data) const {
    return GetImpl()->InitArcIterator(s, data);
  }

  StateId MaxStateId() const {
    return GetImpl()->MaxStateId();
  }

  // Notification that the given state was added to the underlying
  // transducer. Updates the internal StateId mapping.
  StateId AddState(const State *state) {
    return GetImpl()->AddState(state);
  }
  // Notification that the given state was removed from the underlying
  // transducer. Updates the internal StateId mapping.
  StateId RemoveState(const State *state) {
    return GetImpl()->RemoveState(state);
  }

  StateId GetState(const State *state) {
    return GetImpl()->GetState(state);
  }

  const State* GetStateById(StateId id) const {
    return GetImpl()->GetStateById(id);
  }

  void UpdateStartState() {
    GetImpl()->UpdateStartState();
  }
};

}  // namespace trainc

#endif  // FST_INTERFACE_H_
