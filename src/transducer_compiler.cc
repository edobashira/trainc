// transducer_compiler.cc
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

#include <string>
#include "fst/vector-fst.h"
#include "hmm_compiler.h"
#include "transducer.h"
#include "transducer_compiler.h"

namespace trainc {

using fst::StdVectorFst;
using fst::StdArc;


TransducerCompiler::TransducerCompiler()
     : transducer_(NULL),
       boundary_phone_(-1) {}

TransducerCompiler::~TransducerCompiler() {}

StdArc::StateId TransducerCompiler::GetState(
    StdVectorFst *c, const State *state) {
  StateMap::const_iterator si = state_map_.find(state);
  if (si == state_map_.end()) {
    StdArc::StateId id = c->AddState();
    state_map_.insert(std::make_pair(state, id));
    return id;
  } else {
    return si->second;
  }
}

bool TransducerCompiler::IsBoundaryState(
    const State &state, int boundary_phone) {
  if (!state.center().HasElement(boundary_phone))
    return false;
  // check that the boundary symbol occurs in all
  // histories except the most left history
  const PhoneContext &history = state.history();
  bool boundary_in_all_histories = true;
  for (int pos = -1; pos > -history.NumLeftContexts(); --pos) {
    if (!history.GetContext(pos).HasElement(boundary_phone)) {
      boundary_in_all_histories = false;
      break;
    }
  }
  return boundary_in_all_histories;
}

// Create an initial state with the same arcs as the given state, but with
// an epsilon as input symbol (shifted C transducer).
// This ensures that the first CD phone of a sequence of phones has the boundary
// phone in all of its left contexts.
void TransducerCompiler::CreateStartState(
    const State &state, StdVectorFst *c) {
  StdArc::StateId state_id = c->AddState();
  c->SetStart(state_id);
  CreateArcs(state, state_id, true, c);
}

// Create arcs in the C transducer using the arcs of the state in the
// ConstructionalTransducer. If eps_input == true, all input symbols
// are set to epsilon.
void TransducerCompiler::CreateArcs(
    const State &state, StdArc::StateId state_id, bool eps_input,
    StdVectorFst *c) {
  ArcIterator ai(state);
  while (!ai.Done()) {
    const Arc &arc = ai.Value();
    StdArc::Label input = 0;  // epsilon
    if (!eps_input)
      input = GetInputLabel(arc);
    StdArc::Label output = GetOutputLabel(arc);
    StdArc::StateId next_state = GetState(c, arc.target());
    c->AddArc(state_id, StdArc(input, output, StdArc::Weight::One(),
                               next_state));
    ai.Next();
  }
}

StdVectorFst* TransducerCompiler::CreateTransducer() {
  CHECK_NOTNULL(transducer_);
  CHECK_GE(boundary_phone_, 0);
  state_map_.clear();
  StdVectorFst *c = new StdVectorFst();
  StateIterator si(*transducer_);
  bool found_initial = false;
  while (!si.Done()) {
    const State &state = si.Value();
    StdArc::StateId state_id = GetState(c, &state);
    if (IsBoundaryState(state)) {
      // ensure creating only one initial state
      CHECK(!found_initial);
      found_initial = true;
      CreateStartState(state, c);
    }
    if (state.center().HasElement(boundary_phone_)) {
      // Ensure that the last CD phone of a sequence of phones has
      // the boundary phone as right context.
      c->SetFinal(state_id, StdArc::Weight::One());
    }
    CreateArcs(state, state_id, false, c);
    si.Next();
  }
  return c;
}


HmmTransducerCompiler::HmmTransducerCompiler() :
    hmm_compiler_(NULL) {}

StdArc::Label HmmTransducerCompiler::GetInputLabel(const Arc &arc) {
  string input_symbol = hmm_compiler_->GetHmmName(arc.input());
  return hmm_compiler_->GetHmmSymbols().Find(input_symbol);
}

StdArc::Label HmmTransducerCompiler::GetOutputLabel(const Arc &arc) {
  // index shift for phones in ContextBuilder
  return arc.output() + 1;
}

StdVectorFst* HmmTransducerCompiler::CreateTransducer() {
  CHECK_NOTNULL(hmm_compiler_);
  return TransducerCompiler::CreateTransducer();
}


StdArc::Label ModelTransducerCompiler::GetInputLabel(const Arc &arc) {
  LabelMap::const_iterator i = label_map_.find(arc.input());
  if (i == label_map_.end()) {
    fst::StdArc::Label label = label_map_.size() + 1;
    label_map_[arc.input()] = label;
    return label;
  } else {
    return i->second;
  }
}

StdArc::Label ModelTransducerCompiler::GetOutputLabel(const Arc &arc) {
  // index shift for phones in ContextBuilder
  return arc.output() + 1;
}

void ModelTransducerCompiler::GetLabelMap(
    std::vector<const AllophoneModel*> *map) const {
  map->clear();
  map->reserve(label_map_.size() + 1);
  for (LabelMap::const_iterator i = label_map_.begin();
      i != label_map_.end(); ++i) {
    if (i->second >= map->size())
      map->resize(i->second + 1, NULL);
    (*map)[i->second] = i->first;
  }
}

}  // namespace trainc
