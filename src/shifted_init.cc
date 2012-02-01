// shifted_init.cc
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
// Copyright 2012 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)

#include "fst/compose.h"
#include "epsilon_closure.h"
#include "shifted_init.h"

namespace trainc {

void ShiftedLexiconTransducerInitializer::Prepare(const fst::StdExpandedFst &l,
                                                  fst::StdVectorFst *t) const {
  fst::StdVectorFst tb;
  const fst::StdArc::Weight one = fst::StdArc::Weight::One();
  for (int p = 0; p <= num_phones_; ++p) {
    StateId s = tb.AddState();
    CHECK_EQ(s, p);
    if (p == boundary_phone_)
      tb.SetFinal(s, one);
    PhoneMapping::const_iterator map = phone_mapping_.find(p);
    if (map == phone_mapping_.end() || map->second == p) {
      for (int a = 0; a < num_phones_; ++a) {
        StateId target = a;
        PhoneMapping::const_iterator tmap = phone_mapping_.find(a);
        if (tmap != phone_mapping_.end())
          target = tmap->second;
        tb.AddArc(p, fst::StdArc(a + 1, a + 1, one, target));
      }
    }
  }
  fst::StdVectorFst::StateId root = num_phones_;
  tb.SetStart(root);
  tb.SetOutputSymbols(l.InputSymbols());
  fst::Compose(tb, l, t);
}

void ShiftedLexiconTransducerInitializer::Build(const fst::StdExpandedFst &l) {
  CHECK_GE(boundary_phone_, 0);
  VLOG(1) << "# states in initial L: " << l.NumStates();
  fst::StdVectorFst prepared;
  Prepare(l, &prepared);
  LexiconTransducerInitializer::Build(prepared);
  StateContexts contexts;
  EpsilonClosure closure(target_, false, &contexts);
  for (fst::StateIterator<LexiconTransducer> siter(*target_); !siter.Done();
      siter.Next()) {
    closure.AddState(siter.Value());
    State *s = target_->GetStateRef(siter.Value());
    const ContextSet &state_context = contexts.Context(siter.Value());
    int unit = -1;
    for (ContextSet::Iterator i(state_context); !i.Done(); i.Next()) {
      int p = i.Value();
      PhoneMapping::const_iterator m = phone_mapping_.find(p);
      if (m != phone_mapping_.end()) p = m->second;
      if (unit >= 0)
        CHECK_EQ(unit, p);
      else
        unit = p;
    }
    const AllophoneModel *model = NULL;
    if (unit >= 0) {
      model = phone_models_[unit];
      CHECK(find(model->phones().begin(), model->phones().end(), unit)
            != model->phones().end());
    } else {
      CHECK_EQ(s->GetIncomingArcs().size() - s->NumIncomingEpsilons(), 0);
      model = target_->EmptyModel();
    }
    for (LexiconState::ForwardArcIterator aiter(s); !aiter.Done();
        aiter.Next()) {
      if (aiter.Value().model)
        target_->UpdateArc(aiter.Ref(), model);
    }
  }
  State *start = target_->GetStateRef(target_->Start());
  ContextSet *start_context = start->ContextRef()->GetContextRef(0);
  CHECK(start_context->IsEmpty());
  start_context->Add(boundary_phone_);
}

} // namespace trainc

