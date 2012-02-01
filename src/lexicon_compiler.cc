// lexicon_compiler.cc
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

#include "fst/connect.h"
#include "lexicon_compiler.h"
#include "lexicon_state_splitter.h"
#include "state_siblings.h"
#include "hmm_compiler.h"

using std::vector;

namespace trainc {

fst::StdVectorFst* LexiconTransducerCompiler::CreateTransducer() {
  CHECK_NOTNULL(l_);
  CHECK_NOTNULL(hmm_compiler_);
  CHECK_GE(boundary_phone_, 0);
  fst::StdVectorFst *result = new fst::StdVectorFst();
  vector<StateId> start_states;
  for (fst::StateIterator<LexiconTransducer> siter(*l_); !siter.Done();
      siter.Next()) {
    StateId s = siter.Value();
    if (l_->IsStart(s))
      start_states.push_back(s);
    while (s >= result->NumStates())
      result->AddState();
    for (LexiconArcIterator aiter(*l_, s); !aiter.Done(); aiter.Next()) {
      const Arc &arc = aiter.Value();
      fst::StdArc new_arc;
      new_arc.olabel = arc.olabel;
      new_arc.weight = arc.weight;
      new_arc.nextstate = arc.nextstate;
      new_arc.ilabel = GetInputLabel(arc);
      result->AddArc(s, new_arc);
    }
    if (l_->Final(s) != Arc::Weight::Zero())
      result->SetFinal(s, l_->Final(s));
  }
  AddStartState(start_states, result);
  fst::Connect(result);
  return result;
}

// Find the start state. If the original start state has been split, a new
// start state is created and connected with epsilon arcs to all
// initial states which accept the boundary phones as left context
void LexiconTransducerCompiler::AddStartState(
    const vector<StateId> &start_states, fst::StdVectorFst *result) const {
  if (start_states.size() == 1) {
    StateId state = start_states.front();
    result->SetStart(state);
    ContextSet left_context(l_->NumPhones());
    l_->GetSiblings()->GetContext(state, LexiconStateSplitter::kLeftContext,
        &left_context);
    CHECK(left_context.HasElement(boundary_phone_));
  } else {
    CHECK(!l_->IsShifted());
    StateId start = result->AddState();
    result->SetStart(start);
    bool found_start = false;
    ContextSet left_context(l_->NumPhones());
    for (vector<StateId>::const_iterator s = start_states.begin();
        s != start_states.end(); ++s) {
      l_->GetSiblings()->GetContext(*s, LexiconStateSplitter::kLeftContext,
          &left_context);
      if (left_context.HasElement(boundary_phone_)) {
        result->AddArc(start,
            fst::StdArc(0, 0, fst::StdArc::Weight::One(), *s));
        found_start = true;
      }
    }
    CHECK(found_start);
  }
}

fst::StdArc::Label LexiconTransducerCompiler::GetInputLabel(
    const Arc &arc) const {
  if (!arc.model || l_->IsEmptyModel(arc.model)) return 0;
  string input_symbol = hmm_compiler_->GetHmmName(arc.model);
  return hmm_compiler_->GetHmmSymbols().Find(input_symbol);
}

}  // namespace trainc
