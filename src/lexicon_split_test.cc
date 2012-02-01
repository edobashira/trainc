// lexicon_split_test.cc
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

#include "fst/concat.h"
#include "fst/compose.h"
#include "fst/determinize.h"
#include "fst/equivalent.h"
#include "fst/minimize.h"
#include "fst/relabel.h"
#include "fst/rmepsilon.h"
#include "lexicon_check.h"
#include "lexicon_split_test.h"
#include "lexicon_state_splitter.h"
#include "state_siblings.h"
#include "stringutil.h"

namespace trainc {

const int LexiconSplitTest::kNumLeftContext = 1;
const int LexiconSplitTest::kNumRightContext = 1;
const int LexiconSplitTest::kSilPhone = 0;

void LexiconSplitTest::Init(int num_words, int num_phones,
                            bool center_set, bool insert_eps,
                            bool deterministic) {
  ConstructionalTransducerTest::Init(num_phones, kNumLeftContext,
      kNumRightContext, center_set);
  num_words_ = num_words;
  num_phones_ = num_phones;
  l_ = new LexiconTransducer();
  l_->SetContextSize(num_phones_, kNumLeftContext, kNumRightContext,
      center_set);
  for (int p = 0; p < num_phones_; ++p) {
    if (p == kSilPhone) {
      phone_symbols_->AddSymbol("si", p + 1);
    } else {
      phone_symbols_->AddSymbol(StringPrintf("%c", 'a' + p - 1), p + 1);
    }
  }
  if (center_set)
    InitSharedStateTransducer();
  else
    InitTransducer();
  CreateLexicon(insert_eps);
  l_->SetShifted(UseShifted());
  l_->SetSplitDerministic(deterministic);
  l_->Init(*lexicon_, *models_, phone_mapping_, kSilPhone);
}

void LexiconSplitTest::CreateLexicon(bool insert_eps) {
  typedef fst::StdArc::StateId StateId;
  words_.clear();
  words_.resize(num_words_ + 1);
  for (int w = 0; w < num_words_; ++w) {
    int len = (w + 2) / 2;
    Label c = w + 1;
    for (int l = 0; l < len; ++l, ++c)
      words_[w].push_back((c % (num_phones_ - 1)) + 2);
  }
  words_[num_words_].push_back(kSilPhone);
  fst::StdVectorFst lexicon;
  StateId root = lexicon.AddState();
  lexicon.SetStart(root);
  lexicon.SetFinal(root, Weight::One());
  lexicon.SetInputSymbols(NULL);
  Label sil_out = insert_eps ? num_words_ * 2 + 3 : 0;
  lexicon.AddArc(root,
      fst::StdArc(kSilPhone + 1, sil_out, Weight::One(), root));
  for (int w = 0; w < num_words_; ++w) {
    const bool duplicate = insert_eps && !(w % 3);
    for (int d = 0; d < 1 + duplicate; ++d) {
      StateId s = root;
      Label o = insert_eps ? w * 2 + d + 1 : 0;
      for (int l = 0; l < words_[w].size(); ++l) {
        StateId n = lexicon.AddState();
        lexicon.AddArc(s, fst::StdArc(words_[w][l], o, Weight::One(), n));
        s = n;
        o = 0;
      }
      if (insert_eps) {
        lexicon.AddArc(s,
            fst::StdArc(num_phones_ + 1 + d, 0, Weight::One(), root));
      } else {
        lexicon.AddArc(s, fst::StdArc(0, 0, Weight::One(), root));
      }
    }
  }
  lexicon_ = new fst::StdVectorFst;
  fst::Determinize(lexicon, lexicon_);
  fst::Minimize(lexicon_);
  if (insert_eps) {
    vector< pair<Label, Label> > map;
    map.push_back(make_pair(num_phones_ + 1, 0));
    map.push_back(make_pair(num_phones_ + 2, 0));
    fst::Relabel(lexicon_, map, vector< pair<Label,Label> >());
  } else {
    fst::RmEpsilon(lexicon_, true);
  }
}

ModelManager::StateModelRef LexiconSplitTest::GetStateModel(int phone) const {
  ModelManager::StateModelRef state_model =
      models_->GetStateModelsRef()->begin();
  --phone; // phone index shift
  while (state_model != models_->GetStateModelsRef()->end()
      && (*state_model)->GetAllophones().front()->phones().front()
          != phone)
    ++state_model;
  return state_model;
}

void LexiconSplitTest::VerifyTransducer(bool checkWords) {
  LexiconTransducerCheck check(phone_info_);
  check.SetTransducer(l_);
  EXPECT_TRUE(check.IsValid());
  if (checkWords) {
    VerifyComposition();
  }
}

void LexiconSplitTest::VerifyComposition() const {
  fst::StdVectorFst cl;
  CreatePhoneOutput(*l_, &cl);
  for (vector<vector<Label> >::const_iterator word = words_.begin();
      word != words_.end(); ++word) {
    fst::StdVectorFst w, wsi, clw;
    CreateWord(*word, &w);
    wsi = w;
    if (l_->IsShifted())
      AppendSilence(&wsi);
    fst::Compose(cl, wsi, &clw);
    EXPECT_GE(clw.NumStates(), wsi.NumStates());
    for (vector<vector<Label> >::const_iterator word2 = words_.begin();
        word2 != words_.end(); ++word2) {
      fst::StdVectorFst w2, w3 = w;
      CreateWord(*word2, &w2);
      fst::Concat(&w3, w2);
      if (l_->IsShifted())
        AppendSilence(&w3);
      fst::Compose(cl, w3, &clw);
      EXPECT_GE(clw.NumStates(), w3.NumStates());
    }
  }
}

void LexiconSplitTest::CreatePhoneOutput(const LexiconTransducer &l,
                                         fst::StdVectorFst *t) const {
  t->DeleteStates();
  vector<StateId> start_states;
  ContextSet left_context(num_phones_);
  for (fst::StateIterator<LexiconTransducer> siter(l); !siter.Done();
      siter.Next()) {
    LexiconTransducer::StateId s = siter.Value();
    while (s >= t->NumStates()) t->AddState();
    t->SetFinal(s, l.Final(s));
    if (l_->IsStart(s)) {
      if (!l_->IsShifted()) {
        l_->GetSiblings()->GetContext(s, LexiconStateSplitter::kLeftContext,
            &left_context);
        if (left_context.HasElement(kSilPhone))
          start_states.push_back(s);
      } else {
        start_states.push_back(s);
      }
    }
    for (LexiconArcIterator aiter(l, s); !aiter.Done(); aiter.Next()) {
      const LexiconArc &arc = aiter.Value();
      while (arc.nextstate >= t->NumStates()) t->AddState();
      fst::StdArc new_arc;
      new_arc.ilabel = (l_->IsEmptyModel(arc.model) ? 0 : reinterpret_cast<ssize_t>(arc.model));
      new_arc.olabel = arc.model ? arc.ilabel + 1 : 0;
      new_arc.weight = arc.weight;
      new_arc.nextstate = arc.nextstate;
      t->AddArc(s, new_arc);
    }
  }
  EXPECT_GE(start_states.size(), 0);
  if (start_states.size() == 1) {
    t->SetStart(start_states.front());
  } else {
    EXPECT_FALSE(l_->IsShifted());
    StateId start = t->AddState();
    t->SetStart(start);
    for (vector<StateId>::const_iterator s = start_states.begin();
        s != start_states.end(); ++s)
      t->AddArc(start, fst::StdArc(0, 0, Weight::One(), *s));
  }
}

void LexiconSplitTest::CreateWord(const vector<Label> &word,
                                  fst::StdVectorFst *w) const {
  w->DeleteStates();
  StateId s = w->AddState();
  w->SetStart(s);
  for (vector<Label>::const_iterator p = word.begin(); p != word.end();
      ++p) {
    StateId ns = w->AddState();
    w->AddArc(s, fst::StdArc(*p, *p, Weight::One(), ns));
    s = ns;
  }
  w->SetFinal(s, Weight::One());
}

void LexiconSplitTest::AppendSilence(fst::StdVectorFst *f) const {
  fst::StdVectorFst si;
  si.SetStart(si.AddState());
  si.SetFinal(si.AddState(), Weight::One());
  si.AddArc(si.Start(), fst::StdArc(kSilPhone + 1, kSilPhone + 1,
      Weight::One(), 1));
  fst::Concat(f, si);
}

void LexiconSplitTest::Split(ModelManager::StateModelRef state_model,
                             int context_pos, const ContextQuestion &q) {
  int hmm_state = (*state_model)->state();
  AllophoneStateModel::SplitResult new_state_models =
      (*state_model)->Split(context_pos, q);
  ModelSplit split;
  models_->ApplySplit(context_pos, state_model, &new_state_models, &split);
  for (vector<AllophoneModelSplit>::const_iterator s =
      split.phone_models.begin(); s != split.phone_models.end(); ++s) {
    l_->ApplyModelSplit(context_pos, &q, s->old_model, hmm_state,
        s->new_models);
  }
  l_->FinishSplit();
  models_->DeleteOldModels(&split.phone_models);
}

void LexiconSplitTest::TestSplit(int context_pos) {
  VerifyTransducer(false);
  ContextSet qc(num_phones_);
  qc.Add(2);
  ContextQuestion q(qc, "2");
  ModelManager::StateModelRef state_model =
      models_->GetStateModelsRef()->begin();
  int hmm_state = (*state_model)->state();
  AllophoneStateModel::SplitResult new_state_models =
      (*state_model)->Split(context_pos, q);
  ModelSplit split;
  models_->ApplySplit(context_pos, state_model, &new_state_models, &split);
  for (vector<AllophoneModelSplit>::const_iterator s =
      split.phone_models.begin(); s != split.phone_models.end(); ++s) {
    l_->ApplyModelSplit(context_pos, &q, s->old_model, hmm_state,
        s->new_models);
  }
  l_->FinishSplit();
  VerifyTransducer();
}

void LexiconSplitTest::TestSplitEpsilon(int context_pos, int nstates,
                                                  int n_eps_only) {
  Init(5, 5, false);
  bool det = l_->DeterministicSplit();
  delete l_;
  words_.clear();
  l_ = new LexiconTransducer();
  l_->SetContextSize(num_phones_, kNumLeftContext, kNumRightContext, false);
  l_->SetSplitDerministic(det);
  l_->SetShifted(UseShifted());
  {
    fst::StdVectorFst t;
    for (size_t i = 0; i < nstates; ++i)
      t.AddState();
    t.SetStart(0);
    t.SetFinal(nstates-1, Weight::One());
    for (int p = 1; p < num_phones_; ++p) {
      for (size_t s = 0; s < nstates - (2 + n_eps_only); ++s)
        t.AddArc(s, fst::StdArc(p, p, Weight::One(), s+1));
    }
    for (size_t s = 1; s < nstates - 2; ++s)
      t.AddArc(s, fst::StdArc(0, 0, Weight::One(), s+1));
    t.AddArc(
        nstates - 2,
        fst::StdArc(num_phones_, num_phones_, Weight::One(),
            nstates - 1));
    if (context_pos == 1) {
      if (l_->IsShifted()) {
        StateId s = t.AddState();
        t.SetStart(s);
        t.AddArc(s, fst::StdArc(kSilPhone + 1, kSilPhone + 1,
                                Weight::One(), 0));
      }
      fst::StdVectorFst rev_t;
      fst::Reverse(t, &rev_t);
      t = rev_t;
    } else if (l_->IsShifted()) {
      StateId s = t.AddState();
      t.SetFinal(s, Weight::One());
      t.SetFinal(nstates - 1, Weight::Zero());
      t.AddArc(nstates - 1, fst::StdArc(kSilPhone + 1, kSilPhone + 1,
                                        Weight::One(), s));
    }
    l_->Init(t, *models_, phone_mapping_, kSilPhone);
  }
  VerifyTransducer();
  ContextSet qc(num_phones_);
  qc.Add(1);
  ContextQuestion q(qc, "Q");
  ModelManager::StateModelRef state_model = GetStateModel(num_phones_);
  int hmm_state = (*state_model)->state();
  AllophoneStateModel::SplitResult new_state_models =
      (*state_model)->Split(context_pos, q);
  ModelSplit split;
  models_->ApplySplit(context_pos, state_model, &new_state_models, &split);

  for (vector<AllophoneModelSplit>::const_iterator s =
      split.phone_models.begin(); s != split.phone_models.end(); ++s) {
    l_->ApplyModelSplit(context_pos, &q, s->old_model, hmm_state,
        s->new_models);
  }
  l_->FinishSplit();
  VerifyTransducer();
}

void LexiconSplitTest::TestSplitLoop(int context_pos, bool split_loop) {
  Init(5, 5);
  bool det = l_->DeterministicSplit();
  delete l_;
  words_.clear();
  l_ = new LexiconTransducer();
  l_->SetContextSize(num_phones_, kNumLeftContext, kNumRightContext, false);
  l_->SetSplitDerministic(det);
  l_->SetShifted(UseShifted());
  int nstates = 3;
  {
    fst::StdVectorFst t;
    for (size_t i = 0; i < nstates; ++i)
      t.AddState();
    t.SetStart(0);
    t.SetFinal(nstates-1, Weight::One());
    t.AddArc(0,
        fst::StdArc(num_phones_, num_phones_, Weight::One(), 1));
    t.AddArc(1,
        fst::StdArc(num_phones_ - 1, num_phones_ - 1,
            Weight::One(), 1));
    t.AddArc(1,
        fst::StdArc(num_phones_ - 2, num_phones_ - 2,
            Weight::One(), 2));
    t.AddArc(1,
        fst::StdArc(num_phones_ - 3, num_phones_ - 3,
            Weight::One(), 2));
    if (context_pos == -1) {
      if (l_->IsShifted()) {
        t.AddState();
        t.AddArc(nstates, fst::StdArc(kSilPhone + 1, kSilPhone + 1,
            Weight::One(), 0));
        t.SetStart(nstates);
      }
      fst::StdVectorFst rev_t;
      fst::Reverse(t, &rev_t);
      t = rev_t;
      nstates = t.NumStates(); // + 1 for epsilon transition
    } else if (l_->IsShifted()) {
      t.AddState();
      t.AddArc(nstates - 1,
          fst::StdArc(kSilPhone + 1, kSilPhone + 1,
              Weight::One(), nstates));
      t.SetFinal(nstates - 1, Weight::Zero());
      t.SetFinal(nstates, Weight::One());
      ++nstates;
    }
    l_->Init(t, *models_, phone_mapping_, kSilPhone);
  }
  nstates = l_->NumStates();
  ModelManager::StateModelRef state_model = GetStateModel(
      split_loop ? num_phones_ - 1 : num_phones_);
  ContextSet qc(num_phones_);
  qc.Add(num_phones_ - 2);
  ContextQuestion q(qc, "Q");
  Split(state_model, context_pos, q);
  VerifyTransducer();
  if (l_->IsShifted()) {
    if (context_pos == -1)
      EXPECT_EQ(nstates + 1, l_->NumStates());
    else
      EXPECT_EQ(nstates, l_->NumStates());
  } else {
    if (context_pos == -1 || !l_->DeterministicSplit())
      EXPECT_EQ(nstates + 1, l_->NumStates());
    else
      EXPECT_EQ(nstates + 2, l_->NumStates());
  }
}

TEST_F(LexiconSplitTest, InitSmall) {
  Init(10, 5);
  VerifyTransducer(false);
}

TEST_F(LexiconSplitTest, InitLarge) {
  Init(100, 40);
  EXPECT_EQ(l_->NumStates(), lexicon_->NumStates());
  VerifyTransducer(false);
}

TEST_F(LexiconSplitTest, GetStatesForModel) {
  Init(100, 20);
  typedef LexiconTransducer::StateId StateId;
  typedef hash_map<const AllophoneModel*,
      pair<vector<StateId>, vector<StateId> >,
      PointerHash<AllophoneModel> > ModelToStateMap;

  ModelToStateMap states;
  for (fst::StateIterator<LexiconTransducer> siter(*l_); !siter.Done();
      siter.Next()) {
    LexiconTransducer::StateId state_id = siter.Value();
    for (fst::ArcIterator<LexiconTransducer> aiter(*l_, state_id);
        !aiter.Done(); aiter.Next()) {
      const LexiconArc &arc = aiter.Value();
      states[arc.model].first.push_back(arc.prevstate);
      states[arc.model].second.push_back(arc.nextstate);
    }
  }
  for (ModelToStateMap::iterator i = states.begin(); i != states.end(); ++i) {
    vector<StateId> found;
    for (int j = 0; j < 2; ++j) {
      vector<StateId> &set = GetPairElement(i->second, j);
      RemoveDuplicates(&set);
      found.clear();
      l_->GetStatesForModel(i->first, !j, &found);
      EXPECT_TRUE(set == found);
    }
  }
}

TEST_F(LexiconSplitTest, SplitLeft) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplit(-1);
  }
}

TEST_F(LexiconSplitTest, SplitRight) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplit(1);
  }
}

TEST_F(LexiconSplitTest, SplitLeftEpsilon) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitEpsilon(-1, 4, 0);
  }
}

TEST_F(LexiconSplitTest, SplitLeftEpsilonOnly) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitEpsilon(-1, 4, 1);
  }
}

TEST_F(LexiconSplitTest, SplitLeftEpsilonPath) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitEpsilon(-1, 5, 0);
  }
}

TEST_F(LexiconSplitTest, SplitRightEpsilon) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitEpsilon(1, 4, 0);
  }
}

TEST_F(LexiconSplitTest, SplitRightEpsilonOnly) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitEpsilon(1, 4, 1);
  }
}

TEST_F(LexiconSplitTest, SplitRightEpsilonPath) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitEpsilon(1, 5, 0);
  }
}

TEST_F(LexiconSplitTest, SplitBeforeLoop) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitLoop(1, false);
  }
}

TEST_F(LexiconSplitTest, SplitAfterLoop) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitLoop(-1, false);
  }
}

TEST_F(LexiconSplitTest, SplitLoopRight) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitLoop(1, true);
  }
}

TEST_F(LexiconSplitTest, SplitLoopLeft) {
  for (int d = 0; d <=1; ++d) {
    Init(5, 5, false, false, d);
    TestSplitLoop(-1, true);
  }
}

TEST_F(LexiconSplitTest, SplitCenter) {
  Init(5, 5, true);
  VerifyTransducer();
  while (true) {
    ModelManager::StateModelRef state_model =
        models_->GetStateModelsRef()->begin();
    while (state_model != models_->GetStateModelsRef()->end() &&
        (*state_model)->context(0).Size() < 2)
      ++state_model;
    if (state_model == models_->GetStateModelsRef()->end())
      break;
    ContextSet qc(num_phones_);
    qc.Add((*state_model)->GetAllophones().front()->phones().front());
    ContextQuestion q(qc, "Q");
    const AllophoneStateModel *old_model = *state_model;
    int hmm_state = (*state_model)->state();
    AllophoneStateModel::SplitResult new_state_models =
        (*state_model)->Split(0, q);
    ModelSplit split;
    models_->ApplySplit(0, state_model, &new_state_models, &split);

    set<const AllophoneModel*> old_models;
    for (vector<AllophoneModelSplit>::const_iterator s =
        split.phone_models.begin(); s != split.phone_models.end(); ++s) {
      l_->ApplyModelSplit(0, &q, s->old_model, hmm_state, s->new_models);
      old_models.insert(s->old_model);
    }
    l_->FinishSplit();
    for (fst::StateIterator<LexiconTransducer> siter(*l_); !siter.Done();
        siter.Next()) {
      for (LexiconArcIterator aiter(*l_, siter.Value()); !aiter.Done();
          aiter.Next()) {
        const AllophoneModel *m = aiter.Value().model;
        if (m) {
          for (int s = 0; s < m->NumStates(); ++s) {
            const AllophoneStateModel *sm = m->GetStateModel(s);
            EXPECT_NE(sm, old_model);
          }
        }
      }
    }
    VerifyTransducer();
  }
}

TEST_F(LexiconSplitTest, SplitSibling) {
  Init(5, 6);
  CHECK(!l_->IsShifted());
  bool det = l_->DeterministicSplit();
  delete l_;
  words_.clear();
  l_ = new LexiconTransducer();
  l_->SetContextSize(num_phones_, kNumLeftContext, kNumRightContext, false);
  l_->SetSplitDerministic(det);
  l_->SetShifted(false);
  const int nstates = 3;
  {
    fst::StdVectorFst t;
    fst::StdArc::StateId states[nstates];
    for (size_t i = 0; i < nstates; ++i)
      states[i] = t.AddState();
    t.SetStart(states[0]);
    t.SetFinal(states[nstates-1], Weight::One());
    t.AddArc(
        states[0],
        fst::StdArc(num_phones_, num_phones_, Weight::One(),
            states[1]));
    t.AddArc(
        states[0],
        fst::StdArc(num_phones_-1, num_phones_-1, Weight::One(),
            states[1]));
    t.AddArc(
        states[0],
        fst::StdArc(num_phones_-4, num_phones_-4, Weight::One(),
            states[1]));
    t.AddArc(
        states[1],
        fst::StdArc(num_phones_-2, num_phones_-2, Weight::One(),
            states[2]));
    t.AddArc(
        states[1],
        fst::StdArc(num_phones_-3, num_phones_-3, Weight::One(),
            states[2]));
    l_->Init(t, *models_, phone_mapping_, kSilPhone);
  }
  ModelManager::StateModelRef state_model = GetStateModel(num_phones_);
  CHECK(state_model != models_->GetStateModelsRef()->end());
  ContextSet qc(num_phones_);
  qc.Add(num_phones_ - 3);
  ContextQuestion q(qc, "Q");
  Split(state_model, 1, q);
  int new_states = 1;
  if (l_->DeterministicSplit())
    new_states = 2;
  EXPECT_EQ(nstates + new_states, l_->NumStates());
  VerifyTransducer();

  state_model = GetStateModel(num_phones_-1);
  CHECK(state_model != models_->GetStateModelsRef()->end());
  Split(state_model, 1, q);
  EXPECT_EQ(nstates + new_states, l_->NumStates());
  VerifyTransducer();

  state_model = GetStateModel(num_phones_-2);
  CHECK(state_model != models_->GetStateModelsRef()->end());
  ContextSet qcl(num_phones_);
  qcl.Add(1);
  ContextQuestion ql(qcl, "QL");
  Split(state_model, -1, ql);
  if (!l_->DeterministicSplit())
    ++new_states;
  EXPECT_EQ(nstates + new_states, l_->NumStates());
  VerifyTransducer();

  state_model = GetStateModel(num_phones_-4);
  CHECK(state_model != models_->GetStateModelsRef()->end());
  Split(state_model, 1, q);
  VerifyTransducer();
  EXPECT_EQ(nstates + new_states, l_->NumStates());
}

TEST_F(LexiconSplitTest, Split3Phone) {
  for (int d = 0; d <= 1; ++d) {
    for (int np = 5; np < 10; ++np) {
      for (int nw = 2; nw <= 20; nw += 2) {
        TearDown();
        SetUp();
        Init(nw, np, false, false, d);
        VerifyTransducer(false);
        SplitIndividual(100, 10, true);
      }
    }
  }
}

TEST_F(LexiconSplitTest, Split3PhoneShared) {
  for (int d = 0; d <= 1; ++d) {
    for (int np = 5; np < 10; ++np) {
      for (int nw = 2; nw <= 20; nw += 2) {
        TearDown();
        SetUp();
        Init(nw, np, true, false, d);
        SplitIndividual(100, 10, true);
      }
    }
  }
}


TEST_F(LexiconSplitTest, Split3PhoneEpsilon) {
  for (int d = 0; d <= 1; ++d) {
    for (int np = 5; np < 10; ++np) {
      for (int nw = 2; nw <= 20; nw += 2) {
        TearDown();
        SetUp();
        Init(nw, np, false, true, d);
        SplitIndividual(100, 10, true);
      }
    }
  }
}

TEST_F(LexiconSplitTest, Split3PhoneSharedEpsilon) {
  for (int d = 0; d <= 1; ++d) {
    for (int np = 5; np < 10; ++np) {
      for (int nw = 2; nw <= 20; nw += 2) {
        TearDown();
        SetUp();
        Init(nw, np, true, true, d);
        VerifyTransducer();
        SplitIndividual(1, 10, true);
      }
    }
  }
}

}  // namespace trainc
