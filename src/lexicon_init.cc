// lexicon_init.cc
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
#include "fst/dfs-visit.h"
#include "fst/expanded-fst.h"
#include "lexicon_init.h"
#include "lexicon_state_splitter.h"

using std::vector;

namespace trainc {

void LexiconTransducerInitializer::SetModels(const ModelManager &models) {
  typedef ModelManager::StateModelList::const_iterator StateModelIter;
  typedef AllophoneStateModel::AllophoneRefList Allophones;
  const ModelManager::StateModelList &stateModels = models.GetStateModels();
  for (StateModelIter s = stateModels.begin(); s != stateModels.end(); ++s) {
    const Allophones &allophones = (*s)->GetAllophones();
    for (Allophones::const_iterator ai = allophones.begin();
        ai != allophones.end(); ++ai) {
      const AllophoneModel *allophone = *ai;
      const vector<int> &phones = allophone->phones();
      for (vector<int>::const_iterator p = phones.begin(); p != phones.end();
          ++p) {
        if (*p >= phone_models_.size()) phone_models_.resize(*p + 1, NULL);
        phone_models_[*p] = allophone;
      }
    }
  }
}

void LexiconTransducerInitializer::Build(const fst::StdExpandedFst &l) {
  VLOG(1) << "# states in L: " << l.NumStates();
  if (!l.NumStates())
    LOG(FATAL) << "Empty transducer";
  if (HasEpsilonCycle(l))
    LOG(FATAL) << "Initial transducer has input epsilon cycles.";
  vector<StateId> state_map(l.NumStates(), fst::kNoStateId);
  for (fst::StateIterator<fst::StdFst> siter(l); !siter.Done(); siter.Next()) {
    StateId ls = siter.Value();
    if (state_map[ls] == fst::kNoStateId)
      state_map[ls] = target_->AddState();
    StateId state = state_map[ls];
    if (l.Final(ls) != fst::StdArc::Weight::Zero())
      target_->SetFinal(state, l.Final(ls));
    for (fst::ArcIterator<fst::StdFst> aiter(l, ls); !aiter.Done();
        aiter.Next()) {
      const fst::StdArc &arc = aiter.Value();
      if (state_map[arc.nextstate] == fst::kNoStateId)
        state_map[arc.nextstate] = target_->AddState();
      StateId next_state = state_map[arc.nextstate];
      Arc::Label phone = (arc.ilabel ? arc.ilabel - 1 : fst::kNoLabel);
      if (arc.ilabel) {
        DCHECK_LT(phone, phone_models_.size());
        DCHECK(phone_models_[phone]);
      }
      const AllophoneModel *model = (arc.ilabel ? phone_models_[phone] : NULL);
      target_->AddArc(state,
          Arc(phone, arc.olabel, model, arc.weight, next_state));
    }
  }
  for (fst::StateIterator<LexiconTransducer> siter(*target_); !siter.Done();
      siter.Next()) {
    State *s = target_->GetStateRef(siter.Value());
    s->UpdateContext();
  }
  target_->SetStart(state_map[l.Start()]);
}

bool LexiconTransducerInitializer::HasEpsilonCycle(
    const fst::StdExpandedFst &l) const {
  uint64 props = fst::kCyclic | fst::kAcyclic;
  fst::SccVisitor<fst::StdArc> scc(&props);
  fst::DfsVisit(l, &scc, fst::InputEpsilonArcFilter<fst::StdArc>());
  return (props & fst::kCyclic);
}

}  // namespace trainc {
