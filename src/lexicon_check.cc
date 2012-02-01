// lexicon_check.cc
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

#include "lexicon_check.h"
#include "lexicon_transducer.h"

namespace trainc {

void LexiconTransducerCheck::SetTransducer(const LexiconTransducer *l) {
  delete l_;
  l_ = l;
  CHECK_GE(phone_info_->NumPhones(), 0);
  state_context_.clear();
  for (fst::StateIterator<LexiconTransducer> siter(*l_); !siter.Done();
      siter.Next()) {
    LexiconTransducer::StateId state_id = siter.Value();
    const LexiconState* state = l_->GetState(state_id);
    if (state) {
      GetStateContext(state_id, state);
    }
  }
}

void LexiconTransducerCheck::GetStateContext(StateId state_id,
                                             const LexiconState *state) {
  while (state_id >= state_context_.size())
    state_context_.push_back(
        std::make_pair(ContextSet(phone_info_->NumPhones()),
                       ContextSet(phone_info_->NumPhones())));
  ContextSet &left_context = state_context_[state_id].first;
  ContextSet &right_context = state_context_[state_id].second;
  for (LexiconState::ConstForwardArcIterator aiter(state); !aiter.Done();
      aiter.Next()) {
    if (aiter.Value().ilabel != fst::kNoLabel) {
      right_context.Add(aiter.Value().ilabel);
    }
  }
  for (LexiconState::BackwardArcIterator aiter(state); !aiter.Done();
      aiter.Next()) {
    if (aiter.Value().ilabel != fst::kNoLabel)
      left_context.Add(aiter.Value().ilabel);
  }
  std::set<StateId> reachable;
  l_->FindReachable<LexiconState::ConstForwardArcIterator>(
      state_id, &reachable);
  for (std::set<LexiconTransducer::StateId>::const_iterator s =
      reachable.begin(); s != reachable.end(); ++s) {
    for (LexiconState::ConstForwardArcIterator aiter(l_->GetState(*s));
        !aiter.Done(); aiter.Next()) {
      if (aiter.Value().ilabel != fst::kNoLabel) {
        right_context.Add(aiter.Value().ilabel);
      }
    }
  }
  reachable.clear();
  l_->FindReachable<LexiconState::BackwardArcIterator>(
      state_id, &reachable);
  for (std::set<LexiconTransducer::StateId>::const_iterator s =
      reachable.begin(); s != reachable.end(); ++s) {
    for (LexiconState::BackwardArcIterator aiter(l_->GetState(*s));
        !aiter.Done(); aiter.Next()) {
      if (aiter.Value().ilabel != fst::kNoLabel)
        left_context.Add(aiter.Value().ilabel);
    }
  }
}

bool LexiconTransducerCheck::VerifyArc(const PhoneContext &state_context,
                                       const LexiconArc &arc) const {
  if (!arc.model)
    return true;
  const vector<int> &phones = arc.model->phones();
  if (std::find(phones.begin(), phones.end(), arc.ilabel) == phones.end()) {
    VLOG(1) << "arc input label " << arc.ilabel << " does not match model " << arc.model;
    return false;
  }
  int num_phones = phone_info_->NumPhones();
  ContextSet left_phones(num_phones), right_phones(num_phones),
      center_phones(num_phones);
  arc.model->GetCommonContext(-1, &left_phones);
  arc.model->GetCommonContext(1, &right_phones);
  arc.model->GetCommonContext(0, &center_phones);
  if (!phone_info_->IsCiPhone(arc.model->phones().front())) {
    if (!state_context_[arc.prevstate].first.IsSubSet(left_phones)) {
      VLOG(1) << "left state context "
              << ContextSetToString(state_context_[arc.prevstate].first)
              << " not compatible with model "
              << ContextSetToString(left_phones) << " " <<
              arc.model->ToString(true);
      return false;
    }
    if (!state_context_[arc.nextstate].second.IsSubSet(right_phones)) {
      VLOG(1) << "right state context "
              << ContextSetToString(state_context_[arc.nextstate].second)
              << " not compatible with model "
              << ContextSetToString(right_phones) << " "
              << arc.model->ToString(true);
      return false;
    }
  }
  if (!center_phones.HasElement(arc.ilabel)) {
    VLOG(1) << "arc input label not compatible with model";
    return false;
  }
  return true;
}

bool LexiconTransducerCheck::VerifyEmptyModel(StateId state) const {
  if (l_->GetState(state)->GetIncomingArcs().size() != 0) {
    set<StateId> reachable;
    l_->FindReachable<LexiconState::BackwardArcIterator>(state, &reachable);
    for (set<StateId>::const_iterator s = reachable.begin(); s != reachable.end(); ++s) {
      const LexiconState *ps = l_->GetState(*s);
      if (ps->GetIncomingArcs().size() - ps->NumIncomingEpsilons() != 0) {
        VLOG(1) << "empty model found at state " << state << " / " << *s
                << " with incoming arcs";
        return false;
      }
    }
  }
  return true;
}

bool LexiconTransducerCheck::VerifyShiftedArc(const ContextSet &left_context,
                                              const LexiconArc &arc) const {
  if (!arc.model)
    return true;
  if (l_->IsEmptyModel(arc.model))
    return VerifyEmptyModel(arc.prevstate);
  int num_phones = phone_info_->NumPhones();
  ContextSet left_phones(num_phones), right_phones(num_phones),
      center_phones(num_phones);
  arc.model->GetCommonContext(-1, &left_phones);
  arc.model->GetCommonContext(1, &right_phones);
  arc.model->GetCommonContext(0, &center_phones);
  const ContextSet &center_context = state_context_[arc.prevstate].first;
  bool result = true;
  if (!center_context.IsSubSet(center_phones)) {
    VLOG(1) << "center phones "
            << ContextSetToString(center_phones)
            << " is not compatible with state context "
            << ContextSetToString(center_context);
    result = false;
  }
  if (!phone_info_->IsCiPhone(arc.model->phones().front())) {
    if (!right_phones.HasElement(arc.ilabel)) {
      VLOG(1) << "right context "
              << ContextSetToString(right_phones)
              << " is not compatible with input label " << arc.ilabel;
      result = false;
    }
    if (!left_context.IsSubSet(left_phones)) {
      VLOG(1) << "left context "
              << ContextSetToString(left_phones)
              << " is not compatible with reachable context "
              << ContextSetToString(left_context);
      result = false;
    }
  }
  if (!result)
    VLOG(1) << "invalid arc: " << arc.prevstate << " -> " << arc.nextstate
            << "i=" << arc.ilabel << " " << arc.model;
  return result;
}


bool LexiconTransducerCheck::VerifyArcs(StateId state_id) const {
  const LexiconState* state = l_->GetState(state_id);
  const PhoneContext &context = state->Context();
  bool arcs_ok = true;
  for (fst::ArcIterator<LexiconTransducer> aiter(*l_, state_id); !aiter.Done();
      aiter.Next()) {
    arcs_ok = VerifyArc(context, aiter.Value()) && arcs_ok;
  }
  return arcs_ok;
}

bool LexiconTransducerCheck::VerifyShiftedArcs(StateId state_id) const {
  ContextSet left_context(l_->NumPhones());
  set<StateId> reachable;
  l_->FindReachable<LexiconState::BackwardArcIterator>(state_id, &reachable);
  reachable.insert(state_id);
  for (set<StateId>::const_iterator s = reachable.begin(); s != reachable.end();
      ++s) {
    const LexiconState* state = l_->GetState(*s);
    for (LexiconState::BackwardArcIterator aiter(state); !aiter.Done();
        aiter.Next()) {
      if (aiter.Value().model) {
        left_context.Union(state_context_[aiter.Value().prevstate].first);
      }
    }
  }
  bool arcs_ok = true;
  for (fst::ArcIterator<LexiconTransducer> aiter(*l_, state_id); !aiter.Done();
      aiter.Next()) {
    arcs_ok = VerifyShiftedArc(left_context, aiter.Value()) && arcs_ok;
  }
  return arcs_ok;
}

bool LexiconTransducerCheck::VerifyIncoming(
    const LexiconState *state, const ContextSet &left_context) const {
  const LexiconState::ArcRefSet &incoming = state->GetIncomingArcs();
  bool arcs_ok = true;
  for (LexiconState::ArcRefSet::const_iterator i = incoming.begin();
      i != incoming.end(); ++i) {
    const LexiconArc &arc = **i;
    if (arc.model && !left_context.HasElement(arc.ilabel)) {
      VLOG(1) << "incoming arc input label " << arc.ilabel
              << " not compatible with state context "
              << ContextSetToString(left_context);
      arcs_ok = false;
    }
  }
  return arcs_ok;
}

bool LexiconTransducerCheck::IsValid() const {
  CHECK_NOTNULL(l_);
  bool is_valid = true;
  for (fst::StateIterator<LexiconTransducer> siter(*l_); !siter.Done();
      siter.Next()) {
    LexiconTransducer::StateId state_id = siter.Value();
    const LexiconState* state = l_->GetState(state_id);
    if (!state) continue;
    const PhoneContext &state_context = state->Context();
    const ContextSet &right_context = state_context.GetContext(1);
    const ContextSet &left_context = state_context.GetContext(0);
    const ContextSet &full_left_context = state_context_[state_id].first;
    const ContextSet &full_right_context = state_context_[state_id].second;
    if (!left_context.IsSubSet(full_left_context) && !l_->IsStart(state_id)) {
      VLOG(1) << "left context not subset of full left context "
              << "state=" << state_id
              << "left context: {" << ContextSetToString(left_context) << "}"
              << "full context: {" << ContextSetToString(full_left_context)
              << "}";
      is_valid = false;
    }
    if (!right_context.IsSubSet(full_right_context)) {
      VLOG(1) << "right context not subset of full right context";
      is_valid = false;
    }
    bool arcs_ok = l_->IsShifted() ? VerifyShiftedArcs(state_id) :
        VerifyArcs(state_id);
    if (!arcs_ok) {
      VLOG(1) << "arcs of state " << state_id << " not valid";
      is_valid = false;
    }
    if (!l_->IsShifted()) {
      if (!VerifyIncoming(state, left_context)) {
        VLOG(1) << "incoming arcs of state " << state_id << " not valid";
        is_valid = false;
      }
    }
  }
  return is_valid;
}

}  // namespace trainc
