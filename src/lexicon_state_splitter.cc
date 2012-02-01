// lexicon_state_splitter.cc
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

#include "lexicon_state_splitter.h"
#include "epsilon_closure.h"
#include "state_siblings.h"

using std::vector;

namespace trainc {

LexiconStateSplitter::UpdateBase::UpdateBase(LexiconTransducer *l)
    : l_(l), num_phones_(l_->NumPhones()), context_a_(num_phones_),
        context_b_(num_phones_), partition_(context_a_, context_b_) {}

void LexiconStateSplitter::UpdateBase::AddSplit(
    int context_pos, AllophoneModel *old_model,
    const AllophoneModel::SplitResult &new_models, const Partition &context) {
  if (!(context_a_.IsEmpty() && context_b_.IsEmpty())) {
    DCHECK(context.first.IsEqual(context_a_));
    DCHECK(context.second.IsEqual(context_b_));
  } else {
    context_a_ = context.first;
    context_b_ = context.second;
  }
  context_id_ = static_cast<ContextId>(context_pos != -1);
  GetStates(old_model);
  CHECK(splits_.find(old_model) == splits_.end());
  splits_.insert(SplitMap::value_type(old_model, new_models));
}


// TODO(rybach): move to Update, specialize states_ in Shifted splitter
void LexiconStateSplitter::UpdateBase::SetState(StateId old_state, int pos,
                                                StateId state, bool new_state) {
  SplitStateMap::iterator i = states_.find(old_state);
  if (i == states_.end()) {
    pair<SplitStateMap::iterator, bool> ins = states_.insert(
      SplitStateMap::value_type(old_state, SplitStates()));
    i = ins.first;
    for (int c = 0; c < 2; ++c) {
      GetPairElement(i->second.states, c) = fst::kNoStateId;
      GetPairElement(i->second.new_state, c) = false;
    }
  }
  GetPairElement(i->second.states, pos) = state;
  GetPairElement(i->second.new_state, pos) = new_state;
}

// create a new state as a split state for old_state.
// pos is either 0 or 1 and defines the partition index.
LexiconStateSplitter::StateId LexiconStateSplitter::UpdateBase::CreateState(
    StateId old_state, int pos) {
  StateId new_state = l_->AddState();
  if (l_->IsStart(old_state))
    l_->SetStart(new_state);
  l_->SetFinal(new_state, l_->Final(old_state));
  SetState(old_state, pos, new_state, true);
  return new_state;
}

// remove all given incoming arcs, except for arcs with state as predecessor.
void LexiconStateSplitter::UpdateBase::RemoveIncomingArcs(
    StateId state, const ArcRefSet &incoming) {
  for (State::ArcRefSet::const_iterator a = incoming.begin(), n = a;
      a != incoming.end(); a = n) {
    // iterator a will be invalid after RemoveArc, advance iterator beforehand
    ++n;
    if ((*a)->prevstate != state)
      l_->RemoveArc((*a)->prevstate, *a);
  }
}

// add all arcs in add_arcs_ to l_ and clear add_arcs_
void LexiconStateSplitter::UpdateBase::AddArcs() {
  for (vector<Arc>::const_iterator a = add_arcs_.begin(); a != add_arcs_.end();
      ++a)
    l_->AddArc(a->prevstate, *a);
  add_arcs_.clear();
}

// ===========================================================

// Collects and applies a set of AllophoneModel splits
class LexiconStateSplitter::Update : public LexiconStateSplitter::UpdateBase {
  typedef LexiconStateSplitter::UpdateBase UpdateBase;
public:
  Update(LexiconTransducer *l)
      : UpdateBase(l), siblings_(l->GetSiblings()),
          deterministic_(l->DeterministicSplit()) {}
  virtual ~Update() {}

  // Apply all previously added splits
  virtual void Apply();

protected:
  typedef LexiconStateSplitter::State State;
  typedef LexiconStateSplitter::Arc Arc;
  typedef hash_map<const AllophoneModel*, AllophoneModel::SplitResult,
      PointerHash<const AllophoneModel> > SplitMap;
  typedef hash_map<StateId, SplitStates> SplitStateMap;
  typedef hash_set<StateId> StateSet;

  // The modification of the arcs depends on whether the split is applied
  // on left or right contexts.
  virtual void ModifyArcs(const StateSet &states) = 0;
  virtual void GetStates(const AllophoneModel *old_model);

  void UpdateSiblingSplits(const StateSet &states);
  StateId FindSibling(const StateSet &states, StateId old_state, const ContextSet &new_context);

private:
  void Split(const StateSet &states);
  void SplitState(const StateSet &all_states, StateId state,
                  const ContextSet &state_context);
  void RemoveStates();
  void UpdateStates(const StateSet &states);

protected:
  LexiconStateSiblings *siblings_;
  bool deterministic_;
};

void LexiconStateSplitter::Update::Apply() {
  RemoveDuplicates(&states_to_split_);
  StateSet states;
  EpsilonClosure *closure = l_->GetEpsilonClosure(context_id_);
  closure->GetUnion(states_to_split_, &states);
  if (context_a_.IsEmpty()) {
    context_a_ = context_b_;
    context_a_.Invert();
  } else {
    context_b_ = context_a_;
    context_b_.Invert();
  }
  Split(states);
  ModifyArcs(states);
  UpdateStates(states);
  RemoveStates();
}

void LexiconStateSplitter::Update::GetStates(const AllophoneModel *model) {
  l_->GetStatesForModel(model, context_id_ == kLeftContext,
      &states_to_split_, false);
}

// create new split states if required.
void LexiconStateSplitter::Update::Split(const StateSet &states) {
  const StateContexts *contexts = l_->GetStateContexts(context_id_);
  for (StateSet::const_iterator si = states.begin(); si != states.end(); ++si)
    SplitState(states, *si, contexts->Context(*si));
  UpdateSiblingSplits(states);
}

// update split sibling states
void LexiconStateSplitter::Update::UpdateSiblingSplits(
    const StateSet &states) {
  const StateContexts *contexts = l_->GetStateContexts(context_id_);
  for (SplitStateMap::iterator s = states_.begin(); s != states_.end(); ++s) {
    for (int c = 0; c < 2; ++c) {
      StateId &ns = GetPairElement(s->second.states, c);
      bool is_new = GetPairElement(s->second.new_state, c);
      const ContextSet &part = GetPairElement(partition_, c);
      if (ns != fst::kNoStateId && !is_new) {
        // new state is an existing sibling state
        SplitStateMap::const_iterator i = states_.find(ns);
        if (i != states_.end()) {
          // sibling state is split -> use the split state
          ns = GetPairElement(i->second.states, c);
        } else if (states.count(ns) && !contexts->Context(ns).IsSubSet(part)) {
          // sibling state is a valid state -> disable the state which does
          // not match the corresponding context partition
          ns = fst::kNoStateId;
        }
      }
    }
  }
}

// create up to 2 new states for the given state.
// states are created if the given state_context is not compatible with
// the current context partition.
void LexiconStateSplitter::Update::SplitState(
    const StateSet &all_states, StateId state,
    const ContextSet &state_context) {
  ContextSet intersect_a = partition_.first, intersect_b = partition_.second;
  pair<ContextSet, ContextSet> new_state_context(partition_.first,
      partition_.second);
  for (int c = 0; c < 2; ++c)
    GetPairElement(new_state_context, c).Intersect(state_context);
  const bool is_valid = new_state_context.first.IsEmpty() ||
      new_state_context.second.IsEmpty();
  if (!is_valid) {
    for (int c = 0; c < 2; ++c) {
      const ContextSet &new_context = GetPairElement(new_state_context, c);
      if (!new_context.IsEmpty()) {
        StateId s = fst::kNoStateId;
        if (context_id_ == kRightContext && deterministic_) {
          ContextSet max_context(num_phones_);
          siblings_->GetContext(state, context_id_, &max_context);
          max_context.Intersect(GetPairElement(partition_, c));
          s = FindSibling(all_states, state, max_context);
        }
        if (s == fst::kNoStateId) {
          StateId s = CreateState(state, c);
          siblings_->AddState(state, s, context_id_,
              GetPairElement(partition_, c));
        } else {
          SetState(state, c, s, false);
        }
      }
    }
  }
}

LexiconStateSplitter::StateId LexiconStateSplitter::Update::FindSibling(
    const StateSet &all_states, StateId old_state,
    const ContextSet &new_context) {
  DCHECK(context_id_ == kRightContext);
  EpsilonClosure *bwd_closure = l_->GetEpsilonClosure(0);
  bwd_closure->AddState(old_state);
  const ContextSet &left_context = bwd_closure->GetStateContexts()->Context(
      old_state);
  StateId s = siblings_->Find(old_state, left_context, new_context);
  if (s == fst::kNoStateId) {
    // Try to find a sibling state with the old context,
    // which is either valid or will be split, too.
    ContextSet old_context = new_context;
    siblings_->GetContext(old_state, context_id_, &old_context);
    s = siblings_->Find(old_state, left_context, old_context);
    if (s != fst::kNoStateId && (s == old_state || !all_states.count(s)))
      s = fst::kNoStateId;
  }
  return s;
}


// remove all states in remove_states_ from the transducer
void LexiconStateSplitter::Update::RemoveStates() {
  RemoveDuplicates(&remove_states_);
  for (vector<StateId>::const_iterator s = remove_states_.begin();
      s != remove_states_.end(); ++s) {
    siblings_->RemoveState(*s);
    l_->RemoveState(*s);
    states_.erase(*s);
  }
  if (context_id_ == kRightContext) {
    for (SplitStateMap::const_iterator s = states_.begin(); s != states_.end();
        ++s) {
      if (l_->GetState(s->first)->GetIncomingArcs().empty()) {
        siblings_->RemoveState(s->first);
        l_->RemoveState(s->first);
      }
    }
  }
  remove_states_.clear();
}

// update the state context of all old and new states in states_
void LexiconStateSplitter::Update::UpdateStates(const StateSet &all_states) {
  const StateContexts *contexts = l_->GetStateContexts(context_id_);
  for (StateSet::const_iterator s = all_states.begin(); s != all_states.end();
      ++s) {
    l_->GetStateRef(*s)->UpdateContext();
    SplitStateMap::const_iterator split = states_.find(*s);
    if (split != states_.end()) {
      // state has been split
      for (int c = 0; c < 2; ++c) {
        StateId ns = GetPairElement(split->second.states, c);
        if (ns != fst::kNoStateId)
          l_->GetStateRef(ns)->UpdateContext();
      }
    } else {
      // state is valid, only update the context
      for (int c = 0; c < 2; ++c) {
        ContextSet context = contexts->Context(*s);
        const ContextSet &part = GetPairElement(partition_, c);
        context.Intersect(part);
        if (!context.IsEmpty())
          siblings_->UpdateContext(*s, context_id_, part);
      }
    }
  }
}

// ===========================================================

typedef LexiconStateSplitter::ContextId ContextId;

namespace {
// Selects the ArcIterators used by Update::ModifyArcs

template<ContextId> struct SelectIncomingIter {
  typedef LexiconState::BackwardArcIterator Iter;
};
template<ContextId> struct SelectOutgoingIter {
  typedef LexiconState::ForwardArcIterator Iter;
};
template<> struct SelectIncomingIter<LexiconStateSplitter::kLeftContext> {
  typedef LexiconState::ForwardArcIterator Iter;
};
template<> struct SelectOutgoingIter<LexiconStateSplitter::kLeftContext> {
  typedef LexiconState::BackwardArcIterator Iter;
};
}  // namespace

// Context position dependend modification of incoming and outgoing arcs
// of split states.
template<ContextId C>
class LexiconStateSplitter::UpdateTpl : public LexiconStateSplitter::Update {
public:
  UpdateTpl(LexiconTransducer *l) : Update(l) {}
  virtual ~UpdateTpl() {}

protected:
  typedef typename SelectIncomingIter<C>::Iter IncomingArcIter;
  typedef typename SelectOutgoingIter<C>::Iter OutgoingArcIter;

  typedef UpdateBase::StateSet StateSet;
  typedef UpdateBase::SplitStateMap SplitStateMap;
  typedef UpdateBase::SplitMap SplitMap;
  typedef UpdateBase::State State;
  typedef UpdateBase::Arc Arc;

  void ModifyArcs(const StateSet &states);

  void UpdateArcs(const StateSet &states);
  bool UpdateArc(const Arc &arc, const ContextSet &state_context,
                 const SplitStates *new_states, bool *remove_state);
  bool UpdateModel(const Arc &arc, const ContextSet &state_context,
                   const SplitStates *new_states,
                   const AllophoneModel::SplitResult &models,
                   bool *remove_state);
  bool CopyArc(const Arc &arc, const SplitStates *new_states);
  void RedirectArcs(const StateSet &states);
  void RedirectArc(const Arc &arc, const SplitStates &new_states);
  void RedirectEpsilonArc(const Arc &arc, StateId new_state,
                          const SplitStates *new_targets, int part);
};

template<ContextId C>
void LexiconStateSplitter::UpdateTpl<C>::ModifyArcs(const StateSet &states) {
  CHECK_EQ(context_id_, C);
  if (C == LexiconStateSplitter::kRightContext) {
    RedirectArcs(states);
    UpdateArcs(states);
  } else {
    RedirectArcs(states);
    UpdateArcs(states);
  }
}

// update incoming/outgoing arcs of all affected states for a right/left
// context split.
template<ContextId C>
void LexiconStateSplitter::UpdateTpl<C>::UpdateArcs(const StateSet &states) {
  typedef typename IncomingArcIter::ArcAccess ArcAccess;
  vector<State::ArcRef> remove_arcs;
  const StateContexts *contexts = l_->GetStateContexts(C);
  for (StateSet::const_iterator si = states.begin(); si != states.end(); ++si) {
    const StateId old_state = *si;
    const ContextSet &state_context = contexts->Context(old_state);
    State *state = l_->GetStateRef(old_state);
    SplitStateMap::const_iterator new_states = states_.find(old_state);
    const SplitStates *split_states = NULL;
    if (new_states != states_.end())
      split_states = &new_states->second;
    bool remove_state = true, removed_all_arcs = true;
    for (IncomingArcIter aiter(state); !aiter.Done(); aiter.Next()) {
      const Arc &arc = aiter.Value();
      if (UpdateArc(arc, state_context, split_states, &remove_state))
        remove_arcs.push_back(aiter.Ref());
      else
        removed_all_arcs = false;
    }
    if (add_arcs_.empty() && removed_all_arcs)
      remove_state = true;
    if (remove_state)
      remove_states_.push_back(old_state);
    // new arcs are added afterwards to ensure valid arc iterators
    AddArcs();
  }
  RemoveDuplicates(&remove_arcs, ListIteratorCompare<Arc>());
  for (vector<State::ArcRef>::const_iterator a = remove_arcs.begin();
      a != remove_arcs.end(); ++a)
    l_->RemoveArc((*a)->prevstate, *a);
}

// update an arc of an affected state.
// remove_state is set to false if the state should be kept after splitting.
// returns true if the arc should be removed.
template<ContextId C>
bool LexiconStateSplitter::UpdateTpl<C>::UpdateArc(const Arc &arc,
                                     const ContextSet &state_context,
                                     const SplitStates *new_states,
                                     bool *remove_state) {
  typedef typename IncomingArcIter::ArcAccess ArcAccess;
  bool remove_arc = false;
  SplitMap::const_iterator split = splits_.find(arc.model);
  if (split != splits_.end()) {
    // updated model
    UpdateModel(arc, state_context, new_states, split->second, remove_state);
    remove_arc = true;
  } else if ((context_id_ == kLeftContext || !deterministic_) && new_states) {
    // arcs are copied to both new states
    CopyArc(arc, new_states);
    remove_arc = true;
  } else {
    *remove_state = false;
  }
  return remove_arc;
}

// update an arc with a split model.
// remove_state is set to false if the state should be kept after splitting.
// returns true if an arc has been added.
template<ContextId C>
bool LexiconStateSplitter::UpdateTpl<C>::UpdateModel(
    const Arc &arc, const ContextSet &state_context,
    const SplitStates *new_states, const AllophoneModel::SplitResult &models,
    bool *remove_state) {
  typedef typename IncomingArcIter::ArcAccess ArcAccess;
  bool add_arc = false;
  for (int c = 0; c < 2; ++c) {
    const AllophoneModel *new_model = GetPairElement(models, c);
    if (!new_model) continue;
    Arc new_arc = arc;
    new_arc.model = new_model;
    if (new_states) {
      // set the new source state
      StateId new_state = GetPairElement(new_states->states, c);
      ArcAccess::SetSourceState(&new_arc, new_state);
    } else {
      // the source state was not split, only add an arc if the model
      // matches the state context.
      *remove_state = false;
      if (!state_context.IsSubSet(GetPairElement(partition_, c)))
        ArcAccess::SetSourceState(&new_arc, fst::kNoStateId);
    }
    if (ArcAccess::SourceState(new_arc) != fst::kNoStateId) {
      // l_->AddArc(new_arc.prevstate, new_arc);
      add_arcs_.push_back(new_arc);
      add_arc = true;
    }
  }
  return add_arc;
}

// copy the arc to the two new states.
// the arc is not modified.
template<ContextId C>
bool LexiconStateSplitter::UpdateTpl<C>::CopyArc(const Arc &arc,
                                   const SplitStates *new_states) {
  typedef typename IncomingArcIter::ArcAccess ArcAccess;
  bool add_arc = false;
  for (int c = 0; c < 2; ++c) {
    Arc new_arc = arc;
    StateId new_state = GetPairElement(new_states->states, c);
    if (!GetPairElement(new_states->new_state, c)) {
      // state is an existing sibling state -> do not copy arcs
      continue;
    }

    ArcAccess::SetSourceState(&new_arc, new_state);
    if (ArcAccess::SourceState(new_arc) != fst::kNoStateId) {
      // l_->AddArc(new_arc.prevstate, new_arc);
      add_arcs_.push_back(new_arc);
      add_arc = true;
    }
  }
  return add_arc;
}

// redirect outgoing/incoming arcs of all affected states for a right/left
// context split.
template<ContextId C>
void LexiconStateSplitter::UpdateTpl<C>::RedirectArcs(const StateSet &states) {
  typedef typename OutgoingArcIter::ArcAccess ArcAccess;
  vector<State::ArcRef> remove_arcs;
  for (StateSet::const_iterator si = states.begin(); si != states.end(); ++si) {
    State *state = l_->GetStateRef(*si);
    SplitStateMap::const_iterator new_states = states_.find(*si);
    if (new_states == states_.end()) {
      // state is valid, no need to change arcs
      continue;
    }
    const SplitStates &split_states = new_states->second;
    for (OutgoingArcIter aiter(state); !aiter.Done(); aiter.Next()) {
      const Arc &arc = aiter.Value();
      RedirectArc(arc, split_states);
    }
    if (C == kLeftContext) {
      RemoveIncomingArcs(fst::kNoStateId, state->GetIncomingArcs());
    }
    AddArcs();

  }
}

// redirect an arc of an affected state to one of the split states.
template<ContextId C>
void LexiconStateSplitter::UpdateTpl<C>::RedirectArc(
    const Arc &arc, const SplitStates &new_states) {
  typedef typename OutgoingArcIter::ArcAccess ArcAccess;
  StateId old_target = ArcAccess::TargetState(arc);
  SplitStateMap::const_iterator new_targets = states_.find(old_target);
  const SplitStates *split_targets = NULL;
  if (new_targets != states_.end())
    split_targets = &new_targets->second;
  for (int c = 0; c < 2; ++c) {
    StateId new_state = GetPairElement(new_states.states, c);
    bool is_new_state = GetPairElement(new_states.new_state, c);
    if (new_state == fst::kNoStateId)
      continue;
    const ContextSet &context = GetPairElement(partition_, c);
    if (!arc.model) {
      RedirectEpsilonArc(arc, new_state, split_targets, c);
    } else if (is_new_state && context.HasElement(arc.ilabel)) {
      Arc new_arc = arc;
      ArcAccess::SetSourceState(&new_arc, new_state);
      // l_->AddArc(new_arc.prevstate, new_arc);
      add_arcs_.push_back(new_arc);
    }
  }
}

// redirect an epsilon arc to one the split states.
// if the target state of the arc has not been split, the epsilon arc is
// only attached if the target state's context matches.
template<ContextId C>
void LexiconStateSplitter::UpdateTpl<C>::RedirectEpsilonArc(
    const Arc &arc, StateId new_state, const SplitStates *new_targets,
    int part) {
  typedef typename OutgoingArcIter::ArcAccess ArcAccess;
  Arc new_arc = arc;
  ArcAccess::SetSourceState(&new_arc, new_state);
  StateId old_target = ArcAccess::TargetState(arc);
  StateId new_target = fst::kNoStateId;
  const StateContexts *contexts = l_->GetStateContexts(C);
  if (new_targets)
    new_target = GetPairElement(new_targets->states, part);
  else {
    const ContextSet target_context = contexts->Context(old_target);
    if (target_context.IsSubSet(GetPairElement(partition_, part))) {
      new_target = old_target;
    }
  }
  if (new_target != fst::kNoStateId) {
    ArcAccess::SetTargetState(&new_arc, new_target);
    // l_->AddArc(new_arc.prevstate, new_arc);
    add_arcs_.push_back(new_arc);
  }
}

// ===========================================================

LexiconStateSplitter::LexiconStateSplitter(LexiconTransducer *l, int num_phones)
    : l_(l), update_(NULL), num_phones_(num_phones) {}

LexiconStateSplitter::~LexiconStateSplitter() {
  delete update_;
}

// Create an Update object depending on the split context position.
LexiconStateSplitter::UpdateBase* LexiconStateSplitter::CreateUpdate(
    int context_pos) const {
  if (context_pos == -1)
    return new UpdateTpl<kLeftContext>(l_);
  else
    return new UpdateTpl<kRightContext>(l_);
}

// relabel all arcs labels with old_model with one of the models in new_models
// depending on whether the arc's input label matches the models (center)
// context.
void LexiconStateSplitter::RelabelArcs(
    const AllophoneModel *old_model,
    const AllophoneModel::SplitResult &new_models,
    const Partition &new_context) {
  vector<ArcRef> arcs;
  l_->GetArcsForModel(old_model, &arcs);
  for (vector<ArcRef>::const_iterator a = arcs.begin(); a != arcs.end(); ++a) {
    const Arc &arc = **a;
    DCHECK_EQ(arc.model, old_model);
    DCHECK(!(new_context.first.HasElement(arc.ilabel) &&
             new_context.second.HasElement(arc.ilabel)));
    for (int c = 0; c < 2; ++c) {
      const ContextSet &context = GetPairElement(new_context, c);
      const AllophoneModel *new_model = GetPairElement(new_models, c);
      if (new_model && context.HasElement(arc.ilabel)) {
        l_->UpdateArc(*a, new_model);
      }
    }
  }
}

// perform the actual state splitting.
void LexiconStateSplitter::FinishSplit() {
  if (update_) {
    update_->Apply();
    delete update_;
    update_ = NULL;
  }
}

// Apply the given AllophoneModel split to the transducer.
// For left/right context splits, all AllophoneModel splits for an
// AllophoneStateModel split are collected and applied later,
// when FinishSplit() is called.
void LexiconStateSplitter::ApplySplit(
    int context_pos, const ContextQuestion *question,
    AllophoneModel *old_model, int hmm_state,
    const AllophoneModel::SplitResult &new_models) {
  ContextSet context_a(num_phones_), context_b(num_phones_);
  Partition partition(question->GetPhoneSet(0), question->GetPhoneSet(1));
  if (context_pos == 0) {
    RelabelArcs(old_model, new_models, partition);
  } else {
    if (!update_)
      update_ = CreateUpdate(context_pos);
    update_->AddSplit(context_pos, old_model, new_models, partition);
  }
}


}  // namespace trainc
