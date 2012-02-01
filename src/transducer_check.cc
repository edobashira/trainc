// transducer_check.cc
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

#include <set>
#include "fst/compose.h"
#include "fst/vector-fst.h"
#include "fst/symbol-table.h"
#include "phone_models.h"
#include "phone_sequence.h"
#include "transducer.h"
#include "transducer_check.h"

namespace trainc {

using fst::StdVectorFst;
using fst::SymbolTable;
using fst::StdArc;


ConstructionalTransducerCheck::ConstructionalTransducerCheck(
    const ConstructionalTransducer &c, const Phones *phone_info,
    int num_left_contexts, int num_right_contexts)
    : c_(c), phone_info_(phone_info),
      num_left_contexts_(num_left_contexts),
      num_right_contexts_(num_right_contexts),
      all_phones_(phone_info->NumPhones()) {
  all_phones_.Invert();
}

// Check that no output label occours more than once.
bool ConstructionalTransducerCheck::CheckDeterministicOutput(
    const State &state) const {
  bool result = true;
  set<int> seen_output;
  ArcIterator arc_iter(state);
  for (; !arc_iter.Done(); arc_iter.Next()) {
    const Arc &arc = arc_iter.Value();
    if (seen_output.find(arc.output()) != seen_output.end()) {
      REP(WARNING) << "output label occurs more than once: " << arc.output();
      result = false;
    }
  }
  return result;
}

// Check state (h, c) has arcs with phone model (h', c, f)
bool ConstructionalTransducerCheck::CheckPhoneModel(
    const State &state, const Arc &arc) const {
  const AllophoneModel *model = arc.input();
  CHECK_NOTNULL(model);
  for (int s = 0; s < model->NumStates(); ++s) {
    const ContextSet &center_phones = model->GetStateModel(s)->context(0);
    if (!state.center().IsSubSet(center_phones)) {
      REP(WARNING) << "state does not match the model's phone";
      return false;
    }
  }
  return true;
}

// Check that all AllophoneStateModels have a compatible context, i.e.
// the intersection of the context sets of all AllophoneStateModels of
// the arc's AllophoneModel is not empty.
bool ConstructionalTransducerCheck::CheckStateModelCompatibility(
    const State &state, const Arc &arc) const {
  bool result = true;
  PhoneContext common_context(phone_info_->NumPhones(), num_left_contexts_,
                              num_right_contexts_);
  const AllophoneModel &model = *arc.input();
  for (int i = -num_left_contexts_; i <= num_right_contexts_; ++i) {
    if (i == 0) continue;
    common_context.SetContext(i, all_phones_);
  }
  for (int hmm_state = 0; hmm_state < model.NumStates(); ++hmm_state) {
    const AllophoneStateModel &state_model = *model.GetStateModel(hmm_state);
    for (int i = -num_left_contexts_; i <= num_right_contexts_; ++i) {
      if (i == 0) continue;
      common_context.GetContextRef(i)->Intersect(state_model.context(i));
    }
  }
  CHECK(!model.phones().empty());
  int phone = model.phones().front();
  if (!phone_info_->IsCiPhone(phone)) {
    // check that state models have compatible context
    for (int i = -num_left_contexts_; i <= num_right_contexts_; ++i) {
      if (i == 0) continue;
      if (common_context.GetContext(i).IsEmpty()) {
          REP(WARNING) << "state models have no common context for "
                          "context position " << i;
          result = false;
      }
    }
  }
  return result;
}

// Check that the AllophoneStateModels corresponding to the arc's
// AllophoneModel have a context that matches the state's history.
bool ConstructionalTransducerCheck::CheckStateModels(
    const State &state, const Arc &arc) const {
  bool result = true;
  const AllophoneModel &model = *arc.input();
  for (int hmm_state = 0; hmm_state < model.NumStates(); ++hmm_state) {
    const AllophoneStateModel *state_model = model.GetStateModel(hmm_state);
    CHECK(!model.phones().empty());
    int phone = model.phones().front();
    if (!phone_info_->IsCiPhone(phone)) {
      if (!state_model->context(1).HasElement(arc.output())) {
        REP(WARNING) << "arc output " << arc.output()
                     << " does not match right model context";
        result = false;
      }
      // check that state history is more restrictive than
      // the model's left context
      if (!state.GetHistory(-1).IsSubSet(state_model->context(-1))) {
        REP(WARNING) << "state model does not match left model context: "
                     << "position -1";
        result = false;
      }
      for (int pos = 2; pos <= num_left_contexts_; ++pos) {
        if (!state.GetHistory(-pos).IsSubSet(state_model->context(-pos))) {
          REP(WARNING) << "state model does not match left model context: "
                       << "position -" << pos;
          VLOG(2) << state.history().ToString();
          VLOG(2) << state_model->ToString();
          result = false;
        }
      }
    }
  }
  return result;
}

bool ConstructionalTransducerCheck::IsCiPhoneState(const State &state) const {
  const ContextSet &center = state.center();
  for (ContextSet::Iterator p(center); !p.Done(); p.Next()) {
    if (!phone_info_->IsCiPhone(p.Value()))
      return false;
  }
  return true;
}

// Check that the histories of the given state and its successor match.
bool ConstructionalTransducerCheck::CheckTargetState(
    const State &state, const Arc &arc) const {
  bool result = true;
  const State &target_state = *arc.target();
  const bool target_is_ci = IsCiPhoneState(target_state);
  if (!target_is_ci) {
    for (int pos = 2; pos <= num_left_contexts_; ++pos) {
      const ContextSet &state_history = state.GetHistory(-(pos - 1));
      const ContextSet &target_history = target_state.GetHistory(-pos);
      if (!state_history.IsSubSet(target_history)) {
        REP(WARNING) << "invalid state sequence, history mismatch "
                     << "context position -" << pos;
        result = false;
      }
    }
  }
  if (num_left_contexts_ > 1 ||
      !target_is_ci) {
    if (!state.center().IsSubSet(target_state.GetHistory(-1))) {
      REP(WARNING) << "invalid state sequence: history mismatch "
                   << "context position -1";
      result = false;
    }
  }
  if (!target_state.center().HasElement(arc.output())) {
    REP(WARNING) << "arc output is not in to target state's center context: "
                 << arc.output();
    result = false;
  }
  return result;
}

// Runs all tests for all states and all arcs.
bool ConstructionalTransducerCheck::IsValid() const {
  bool result = true;
  StateIterator state_iter(c_);
  for (; !state_iter.Done(); state_iter.Next()) {
    const State &state = state_iter.Value();
    result &= CheckDeterministicOutput(state);
    ArcIterator arc_iter(state);
    for (; !arc_iter.Done(); arc_iter.Next()) {
      const Arc &arc = arc_iter.Value();
      result &= CheckPhoneModel(state, arc);
      result &= CheckStateModelCompatibility(state, arc);
      result &= CheckStateModels(state, arc);
      result &= CheckTargetState(state, arc);
    }
  }
  return result;
}


CTransducerCheck::~CTransducerCheck() {
  delete piter_;
  delete hmms_;
  delete phones_;
}

void CTransducerCheck::Init(
    const std::string &phone_symbols, const std::string &hmm_symbols,
    const std::string &hmm_to_phone, const std::string &boundary_phone,
    int context_length) {
  phones_ = SymbolTable::ReadText(phone_symbols);
  CHECK(phones_);
  hmms_ = SymbolTable::ReadText(hmm_symbols);
  CHECK(hmms_);
  hmm_to_phone_.LoadMap(hmm_to_phone);
  length_ = context_length;
  boundary_phone_ = phones_->Find(boundary_phone);
  CHECK_GT(boundary_phone_, 0);
  piter_ = new PhoneSequenceIterator(length_, phones_);
}

void CTransducerCheck::SetTransducer(const StdVectorFst *c) {
  c_ = c;
  CHECK(c_);
}

bool CTransducerCheck::IsValid() const {
  while (!piter_->Done()) {
    StdVectorFst phone_fst;
    std::vector<int> phone_vec;
    piter_->TransducerValue(&phone_fst);
    piter_->IndexValue(&phone_vec);
    AddBoundaryPhone(&phone_fst);
    bool valid = CheckPhoneSequence(phone_fst, phone_vec);
    if (!valid) {
      return false;
    }
    piter_->Next();
  }
  return true;
}

void CTransducerCheck::AddBoundaryPhone(StdVectorFst *phone_fst) const {
  StdVectorFst::StateId new_final = phone_fst->AddState();
  StdVectorFst::StateId final = fst::kNoStateId;
  // find unique final state
  fst::StateIterator<StdVectorFst> siter(*phone_fst);
  siter.Next();  // skip start state
  while (!siter.Done()) {
    StdVectorFst::StateId s = siter.Value();
    if (phone_fst->Final(s) != StdArc::Weight::Zero()) {
      CHECK_EQ(final, fst::kNoStateId);
      final = s;
    }
    siter.Next();
  }
  CHECK_NE(final, fst::kNoStateId);
  // add new arc;
  StdArc narc(boundary_phone_, boundary_phone_, StdArc::Weight::One(),
              new_final);
  phone_fst->AddArc(final, narc);
  // set new final state
  phone_fst->SetFinal(final, StdArc::Weight::Zero());
  phone_fst->SetFinal(new_final, StdArc::Weight::One());
}

void CTransducerCheck::GetSequence(
    const StdVectorFst &cl, vector<int> *hmm_seq) const {
  StdVectorFst::StateId s = cl.Start();
  hmm_seq->clear();
  while (cl.NumArcs(s) > 0) {
    CHECK_LE(cl.NumArcs(s), 1);
    fst::ArcIterator<StdVectorFst> ai(cl, s);
    const StdArc &arc = ai.Value();
    hmm_seq->push_back(arc.ilabel);
    s = arc.nextstate;
  }
}


bool CTransducerCheck::CheckPhoneSequence(
    const StdVectorFst &phone_fst, const vector<int> &phone_seq) const {
  StdVectorFst cl;
  fst::Compose(*c_, phone_fst, &cl);
  CHECK_EQ(cl.NumStates(), length_ + 2);
  if (VLOG_IS_ON(1))
    PrintSequence(cl);
  vector<int> hmm_seq;
  GetSequence(cl, &hmm_seq);
  CHECK_EQ(hmm_seq.size(), length_ + 1);
  CHECK_EQ(hmm_seq[0], 0);
  for (int p = 0; p < length_; ++p) {
    string cd_phone = hmm_to_phone_.get(hmms_->Find(hmm_seq[p+1]));
    string phone = phones_->Find(phone_seq[p]);
    if (cd_phone != phone) {
      return false;
    }
  }
  return true;
}


void CTransducerCheck::PrintSequence(const StdVectorFst &cl) const {
  StdVectorFst::StateId s = cl.Start();
  string input, output;
  VLOG(1) << "num states: " << cl.NumStates();
  while (cl.NumArcs(s) > 0) {
    CHECK_LE(cl.NumArcs(s), 1);
    fst::ArcIterator<StdVectorFst> ai(cl, s);
    const StdArc &arc = ai.Value();
    const string hmm_name = hmms_->Find(arc.ilabel);
    input += hmm_name + "/";
    input +=  (arc.ilabel ? hmm_to_phone_.get(hmm_name) : "eps") + " ";
    output += phones_->Find(arc.olabel) + " ";
    s = arc.nextstate;
  }
  VLOG(1) << "input:  " << input;
  VLOG(1) << "output: " << output;
}


}  // namespace trainc
