// fst_interface.cc
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

#include "fst_interface.h"
#include "transducer_compiler.h"

using fst::SymbolTable;

namespace trainc {

const string FstInterfaceImpl::kType = "constructional-c";

const uint64 FstInterfaceImpl::kProperties = fst::kExpanded |
    fst::kNotAcceptor | fst::kNonIDeterministic | fst::kODeterministic |
    fst::kIEpsilons | fst::kNoOEpsilons | fst::kNotILabelSorted |
    fst::kNotOLabelSorted | fst::kUnweighted | fst::kCyclic |
    fst::kInitialAcyclic | fst::kNotTopSorted | fst::kAccessible |
    fst::kCoAccessible | fst::kNotString;

const FstInterfaceImpl::StateId FstInterfaceImpl::kRootId = 0;

void FstInterfaceImpl::Init(ConstructionalTransducer *c, int boundary_phone) {
  c_ = c;
  boundary_phone_ = boundary_phone;
  id2state_.reserve(c->NumStates() + 1);
  state_ids_.resize(c->NumStates() + 1);
  PhoneContext root_context(c_->NumPhones(), c_->NumLeftContexts(), 0);
  for (int l = 0; l > -c_->NumLeftContexts(); --l)
    root_context.GetContextRef(l)->Add(boundary_phone);
  root_ = new State(root_context);
  // VLOG(2) << "root: " << root_->history().ToString();
  CHECK_EQ(GetStateId(root_), kRootId);
  for (CStateIterator siter(*c_); !siter.Done(); siter.Next()) {
    GetStateId(&siter.Value());
  }
  UpdateStartState();
}

FstInterfaceImpl::StateId FstInterfaceImpl::GetStateId(
    const State *state, bool add) {
  StateMap::const_iterator i = state_ids_.find(state);
  if (i != state_ids_.end()) {
    return i->second;
  } else if (add) {
    StateId id;
    if (free_ids_.empty()) {
      id = id2state_.size();
      id2state_.push_back(state);
    } else {
      id = free_ids_.back();
      free_ids_.pop_back();
      id2state_[id] = state;
    }
    state_ids_.insert(StateMap::value_type(state, id));
    // VLOG(2) << "state id: " << state << " " << id;
    return id;
  } else {
    return fst::kNoStateId;
  }
}

FstInterfaceImpl::StateId FstInterfaceImpl::MaxStateId() const {
  return id2state_.size() - 1;
}

const State* FstInterfaceImpl::FindBoundaryState() const {
  const State* s = NULL;
  for (CStateIterator siter(*c_); !siter.Done(); siter.Next()) {
    if (TransducerCompiler::IsBoundaryState(siter.Value(), boundary_phone_)) {
      s = &siter.Value();
      break;
    }
  }
  CHECK_NOTNULL(s);
  return s;
}

void FstInterfaceImpl::UpdateStartState() {
  if (!boundary_state_)
    boundary_state_ = FindBoundaryState();
  root_->ClearArcs();
  for (CArcIterator aiter(*boundary_state_); !aiter.Done(); aiter.Next()) {
    const CArc &arc = aiter.Value();
    root_->AddArc(NULL, arc.output(), arc.target());
  }
}

FstInterfaceImpl::Weight FstInterfaceImpl::Final(StateId s) const {
  DCHECK_LT(s, id2state_.size());
  DCHECK(std::find(free_ids_.begin(), free_ids_.end(), s) == free_ids_.end());
  if (!id2state_[s]->center().HasElement(boundary_phone_))
    return Arc::Weight::Zero();
  else
    return Arc::Weight::One();
}

size_t FstInterfaceImpl::NumArcs(StateId s) const {
  DCHECK_LT(s, id2state_.size());
  DCHECK(std::find(free_ids_.begin(), free_ids_.end(), s) == free_ids_.end());
  return id2state_[s]->GetArcs().size();
}

size_t FstInterfaceImpl::NumInputEpsilons(StateId s) const {
  if (s != kRootId)
    return 0;
  else
    return root_->GetArcs().size();
}

FstInterfaceImpl::StateId FstInterfaceImpl::AddState(const State *state) {
  StateId id = GetStateId(state);
  // VLOG(2) << "add state: " << state << " " << id;
  return id;
}

FstInterfaceImpl::StateId FstInterfaceImpl::RemoveState(const State *state) {
  StateId id = GetStateId(state);
  // VLOG(2) << "remove state " << state << " " << id;
  StateMap::iterator i = state_ids_.find(state);
  DCHECK(i != state_ids_.end());
  state_ids_.erase(i);
  free_ids_.push_back(id);
  if (state == boundary_state_)
    boundary_state_ = NULL;
  return id;
}

FstInterfaceImpl::StateId FstInterfaceImpl::GetState(const State *state) {
  return GetStateId(state, false);
}

const State* FstInterfaceImpl::GetStateById(StateId id) const {
  DCHECK_LT(id, id2state_.size());
  return id2state_[id];
}

class FstInterfaceImpl::StateIterator :
    public fst::StateIteratorBase<fst::StdArc> {
public:
  StateIterator(const FstInterfaceImpl &fst) : ids_(fst.state_ids_) {
    Reset_();
  }
private:
  bool Done_() const {
    return iter_ == end_;
  }
  virtual StateId Value_() const {
    return iter_->second;
  }
  virtual void Next_() {
    ++iter_;
  }
  virtual void Reset_() {
    iter_ = ids_.begin();
    end_ = ids_.end();
  }
  const FstInterfaceImpl::StateMap &ids_;
  FstInterfaceImpl::StateMap::const_iterator iter_, end_;
};

class FstInterfaceImpl::ArcIterator :
    public fst::ArcIteratorBase<fst::StdArc> {
  typedef trainc::ArcIterator CArcIterator;
  typedef fst::StdArc Arc;
public:
  ArcIterator(CArcIterator iter, const FstInterfaceImpl &fst) {
    // TODO(rybach) cache arcs
    for (; !iter.Done(); iter.Next()) {
      const CArc &arc = iter.Value();
      StateMap::const_iterator next = fst.state_ids_.find(arc.target());
      DCHECK(next != fst.state_ids_.end());
      arcs_.push_back(Arc(0, arc.output() + 1, Arc::Weight::One(), next->second));
    }
    iter_ = arcs_.begin();
  }
private:
  virtual bool Done_() const {
    return iter_ == arcs_.end();
  }
  virtual const Arc& Value_() const {
    return *iter_;
  }
  virtual void Next_() {
    ++iter_;
  }
  virtual size_t Position_() const {
    return std::distance(arcs_.begin(), iter_);
  }
  virtual void Reset_() {
    iter_ = arcs_.begin();
  }
  virtual void Seek_(size_t a) {
    iter_ = arcs_.begin() + a;
  }
  virtual uint32 Flags_() const {
    return fst::kArcFlags;
  }
  virtual void SetFlags_(uint32 flags, uint32 mask) {}
  std::vector<Arc> arcs_;
  std::vector<Arc>::const_iterator iter_;
};

void FstInterfaceImpl::InitStateIterator(
    fst::StateIteratorData<Arc> *data) const {
  data->base = new FstInterfaceImpl::StateIterator(*this);
  data->nstates = state_ids_.size();
}
void FstInterfaceImpl::InitArcIterator(
    StateId s, fst::ArcIteratorData<Arc> *data) const {
  DCHECK_LT(s, id2state_.size());
  const State *state = id2state_[s];
  data->base = new FstInterfaceImpl::ArcIterator(CArcIterator(*state), *this);
}

}  // namespace trainc
