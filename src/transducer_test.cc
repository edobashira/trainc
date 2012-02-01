// transducer_test.cc
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
//
// Test basic splitting of transducer states and phone (state) models.
// No phone model statistics (GaussStats / SuffStat) are used by these tests.

#include <set>
#include "transducer_test.h"
#include "phone_models.h"
#include "split_predictor.h"
#include "transducer_check.h"
#include "transducer_init.h"

namespace trainc {

void ConstructionalTransducerTest::Init(
    int num_phones, int num_left_contexts, int num_right_contexts,
    bool center_set) {
  num_phones_ = num_phones;
  num_left_contexts_ = num_left_contexts;
  num_right_contexts_ = num_right_contexts;
  center_set_ = center_set;
  c_ = new ConstructionalTransducer(num_phones_, num_left_contexts_,
                                    num_right_contexts_, center_set_);
  models_ = new ModelManager();
  phone_info_ = new Phones(num_phones_);
  all_phones_ = new ContextSet(num_phones_);
  phone_info_->SetCiPhone(0);
  phone_info_->SetPhoneLength(0, 1);
  for (int p = 0; p < num_phones_; ++p) {
    all_phones_->Add(p);
    if (p > 0) phone_info_->SetPhoneLength(p, 3);
  }
  phone_mapping_.clear();
  if (center_set) {
    phone_mapping_[2] = 1;
    phone_mapping_[4] = 3;
  }
}

void ConstructionalTransducerTest::TearDown() {
  delete phone_info_;
  delete all_phones_;
  delete models_;
  delete c_;
}

void ConstructionalTransducerTest::CreatePhoneSets(
    ContextSet *a, ContextSet *b) {
  ASSERT_EQ(a->Capacity(), num_phones_);
  ASSERT_EQ(b->Capacity(), num_phones_);
  for (int p = 0; p < num_phones_; p += 3)
    a->Add(p);
  for (int p = 0; p < num_phones_; p += 4)
    b->Add(p);
}


void ConstructionalTransducerTest::VerifyTransducer() {
  EXPECT_TRUE(c_);
  EXPECT_TRUE(ConstructionalTransducerCheck(*c_, phone_info_,
                             num_left_contexts_,
                             num_right_contexts_).IsValid());
}

void ConstructionalTransducerTest::InitTransducer() {
  BasicTransducerInitialization init;
  init.SetPhoneInfo(phone_info_);
  init.SetContextLenghts(num_left_contexts_, num_right_contexts_);
  init.SetAnyPhoneContext(all_phones_);
  ASSERT_TRUE(init.Prepare());
  init.CreateModels(models_);
  init.Execute(c_);
}

void ConstructionalTransducerTest::InitSharedStateTransducer() {
  SharedStateTransducerInitialization init;
  init.SetPhoneInfo(phone_info_);
  init.SetContextLenghts(num_left_contexts_, num_right_contexts_);
  init.SetAnyPhoneContext(all_phones_);
  ASSERT_GE(num_phones_, 5);
  init.SetPhoneMap(phone_mapping_);
  ASSERT_TRUE(init.Prepare());
  init.CreateModels(models_);
  init.Execute(c_);
}


void ConstructionalTransducerTest::VerifyModels() {
  typedef AllophoneStateModel::AllophoneRefList::const_iterator AmIter;
  typedef ModelManager::StateModelList::const_iterator SmIter;
  for (SmIter sm_iter = models_->GetStateModels().begin();
      sm_iter != models_->GetStateModels().end(); ++sm_iter) {
    const AllophoneStateModel *state_model = *sm_iter;
    int hmm_state = state_model->state();
    int prev_phone = -1;
    for (AmIter am_iter = state_model->GetAllophones().begin();
        am_iter != state_model->GetAllophones().end(); ++am_iter) {
      const AllophoneModel *model = *am_iter;
      EXPECT_TRUE(model->GetStateModel(hmm_state) == state_model);
      int phone = model->phones().front();
      if (prev_phone >= 0) EXPECT_EQ(prev_phone, phone);
      for (int pos = -num_left_contexts_; pos <= num_right_contexts_; ++pos) {
        ContextSet cc(num_phones_);
        model->GetCommonContext(pos, &cc);
        if (pos != 0 && !phone_info_->IsCiPhone(phone))
          EXPECT_FALSE(cc.IsEmpty());
      }
      for (int s = 0; s < model->NumStates(); ++s) {
        EXPECT_EQ(s, model->GetStateModel(s)->state());
      }
      prev_phone = phone;
    }
  }
}
void ConstructionalTransducerTest::SplitOneModel(int position) {
    ModelManager::StateModelRef state_model =
        models_->GetStateModelsRef()->begin();
  ContextSet s(num_phones_);
  s.Add(0);
  s.Add(num_phones_ - 1);
  ContextQuestion question(s);
  int hmm_state = (*state_model)->state();
  AllophoneStateModel::SplitResult new_state_models =
      (*state_model)->Split(position, question);
  ModelSplit split;
  models_->ApplySplit(position, state_model, &new_state_models, &split);
  for (vector<AllophoneModelSplit>::const_iterator s =
      split.phone_models.begin(); s != split.phone_models.end(); ++s) {
    GetC()->ApplyModelSplit(position, &question, s->old_model, hmm_state,
        s->new_models);
  }
  GetC()->FinishSplit();
  models_->DeleteOldModels(&split.phone_models);
  VerifyTransducer();
  VerifyModels();
}

void ConstructionalTransducerTest::SplitAllModels(int position, ContextSet *s) {
  bool delete_s = false;
  if (s == NULL) {
    s = new ContextSet(num_phones_);
    delete_s = true;
    s->Add(0);
    s->Add(num_phones_ - 1);
  }
  ContextQuestion question(*s);
  if (delete_s) delete s;
  ModelManager::StateModelList &state_models = *models_->GetStateModelsRef();
  int num_state_models = state_models.size();
  ModelManager::StateModelRef sm_iter = state_models.begin(),
      sm_end = state_models.end();
  while (sm_iter != sm_end) {
    AllophoneStateModel *state_model = *sm_iter;
    int phone = state_model->GetAllophones().front()->phones().front();
    if (phone_info_->IsCiPhone(phone)) {
      ++sm_iter;
      continue;
    }
    int hmm_state = state_model->state();
    AllophoneStateModel::SplitResult new_state_models =
        state_model->Split(position, question);
    ModelSplit split;
    sm_iter = models_->ApplySplit(position, sm_iter, &new_state_models, &split);
    for (vector<AllophoneModelSplit>::const_iterator s =
        split.phone_models.begin(); s != split.phone_models.end(); ++s) {
      GetC()->ApplyModelSplit(position, &question, s->old_model, hmm_state,
          s->new_models);
    }
    GetC()->FinishSplit();
    models_->DeleteOldModels(&split.phone_models);
    VerifyTransducer();
    VerifyModels();
  }
  EXPECT_GE(state_models.size(), num_state_models);
}

void ConstructionalTransducerTest::SplitIndividual(
    int niter, int nquestions, bool check_count) {
  vector<ContextQuestion*> questions;
  for (int i = 0; i < nquestions; ++i) {
    ContextSet set(num_phones_);
    for (int p = 0; p < num_phones_; ++p)
      if (p % (i + 2) == 0) set.Add(p);
    questions.push_back(new ContextQuestion(set));
  }
  ASSERT_EQ(questions.size(), nquestions);
  ModelManager::StateModelList &state_models = *models_->GetStateModelsRef();
  ModelManager::StateModelRef sm_iter = state_models.begin();
  AbstractSplitPredictor *predictor = GetC()->CreateSplitPredictor();
  predictor->SetDiscardAbsentModels(false);
  int offset = 3;
  int position = 1;
  int num_state_models = 0;
  const int context_size = num_left_contexts_ + num_right_contexts_;
  for (int iter = 0; iter < niter; ++iter) {
    position = (iter % context_size) - num_left_contexts_;
    if (!position) position = 1;
    num_state_models = state_models.size();
    AllophoneStateModel *state_model = *sm_iter;
        int phone = state_model->GetAllophones().front()->phones().front();
    int qidx = (iter + offset) % questions.size();
    ASSERT_LT(qidx, questions.size());
    const ContextQuestion &question = *questions[qidx];
    int predict_states = 0;
    int prev_num_states  = 0;
    if (!phone_info_->IsCiPhone(phone)) {
      int hmm_state = state_model->state();
      if (check_count) {
        prev_num_states = GetC()->NumStates();
        predictor->Init();
        predict_states = predictor->Count(
            position, question, state_model->GetAllophones(), 0);
      }
      AllophoneStateModel::SplitResult new_state_models =
          state_model->Split(position, question);
      if (new_state_models.first && new_state_models.second) {
        ModelSplit split;
        models_->ApplySplit(position, sm_iter, &new_state_models, &split);
        for (vector<AllophoneModelSplit>::const_iterator s =
            split.phone_models.begin(); s != split.phone_models.end(); ++s) {
          GetC()->ApplyModelSplit(position, &question, s->old_model, hmm_state,
              s->new_models);
        }
        GetC()->FinishSplit();
        models_->DeleteOldModels(&split.phone_models);
        VerifyTransducer();
        VerifyModels();
        if (check_count) {
          VLOG(2) << "predicted: " << (prev_num_states + predict_states)
                  << " found: " << GetC()->NumStates();
          EXPECT_EQ(GetC()->NumStates(), prev_num_states + predict_states);
        }
      }
    }
    int new_size = state_models.size();
    EXPECT_GE(new_size, num_state_models);
    offset = (offset * 7 + 5) % new_size;
    sm_iter = state_models.begin();
    advance(sm_iter, offset);
  }
  for (vector<ContextQuestion*>::iterator q = questions.begin();
      q != questions.end(); ++q)
    delete *q;
  delete predictor;
  VLOG(1) << "number of state models: " << state_models.size();
}

TEST_F(ConstructionalTransducerTest, CheckBasicInit3) {
  Init(10, 1, 1);
  InitTransducer();
  VerifyTransducer();
}

TEST_F(ConstructionalTransducerTest, CheckBasicInit4) {
  Init(10, 2, 1);
  InitTransducer();
  VerifyTransducer();
}

TEST_F(ConstructionalTransducerTest, CheckBoundaryInit3) {
  Init(10, 1, 1);
  WordBoundaryTransducerInitialization init;
  init.SetPhoneInfo(phone_info_);
  init.SetContextLenghts(num_left_contexts_, num_right_contexts_);
  init.SetAnyPhoneContext(all_phones_);
  vector<int> initial_phones, final_phones;
  initial_phones.push_back(0);
  initial_phones.push_back(1);
  final_phones.push_back(0);
  final_phones.push_back(2);
  map<int, int> phone_map;
  phone_map[1] = 3;
  phone_map[2] = 4;
  init.SetPhoneMap(phone_map);
  init.SetInitialPhones(initial_phones);
  init.SetFinalPhones(final_phones);
  ASSERT_TRUE(init.Prepare());
  init.CreateModels(models_);
  init.Execute(c_);
}


TEST_F(ConstructionalTransducerTest, SplitOneFuture) {
  Init(10, 1, 1);
  InitTransducer();
  SplitOneModel(1);
}

TEST_F(ConstructionalTransducerTest, SplitAllFuture) {
  Init(10, 1, 1);
  InitTransducer();
  SplitAllModels(1);
}

TEST_F(ConstructionalTransducerTest, SplitOneHistory3) {
  Init(10, 1, 1);
  InitTransducer();
  SplitOneModel(-1);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistory3) {
  Init(10, 1, 1);
  InitTransducer();
  SplitAllModels(-1);
}

TEST_F(ConstructionalTransducerTest, SplitOneHistory4First) {
  Init(10, 2, 1);
  InitTransducer();
  SplitOneModel(-1);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistory4First) {
  Init(10, 2, 1);
  InitTransducer();
  SplitAllModels(-1);
}

TEST_F(ConstructionalTransducerTest, SplitOneHistory4Second) {
  Init(10, 2, 1);
  InitTransducer();
  SplitOneModel(-2);
}


TEST_F(ConstructionalTransducerTest, SplitAllHistory4Second) {
  Init(10, 2, 1);
  InitTransducer();
  SplitAllModels(-2);
}

TEST_F(ConstructionalTransducerTest, SplitOneHistory5Phone) {
  Init(10, 3, 1);
  InitTransducer();
  SplitOneModel(-3);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistory5Phone) {
  Init(10, 3, 1);
  InitTransducer();
  SplitAllModels(-3);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistoryTwice3) {
  Init(10, 1, 1);
  InitTransducer();
  ContextSet a(num_phones_), b(num_phones_);
  CreatePhoneSets(&a, &b);
  SplitAllModels(-1, &a);
  SplitAllModels(-1, &b);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistoryTwice4) {
  Init(10, 2, 1);
  InitTransducer();
  ContextSet a(num_phones_), b(num_phones_);
  CreatePhoneSets(&a, &b);
  SplitAllModels(-1, &a);
  SplitAllModels(-1, &b);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistoryTwice4Second) {
  Init(10, 2, 1);
  InitTransducer();
  ContextSet a(num_phones_), b(num_phones_);
  CreatePhoneSets(&a, &b);
  SplitAllModels(-2, &a);
  SplitAllModels(-2, &b);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistoryTwice4FirstAndSecond) {
  Init(10, 2, 1);
  InitTransducer();
  ContextSet a(num_phones_), b(num_phones_);
  CreatePhoneSets(&a, &b);
  SplitAllModels(-1, &a);
  SplitAllModels(-2, &b);
  SplitAllModels(-1, &b);
  SplitAllModels(-2, &a);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistoryAndFuture) {
  Init(10, 2, 1);
  InitTransducer();
  ContextSet a(num_phones_), b(num_phones_);
  CreatePhoneSets(&a, &b);
  SplitAllModels(-1, &a);
  SplitAllModels(1, &b);
  SplitAllModels(-2, &b);
  SplitAllModels(1, &a);
  SplitAllModels(-1, &b);
  SplitAllModels(-2, &a);
}

TEST_F(ConstructionalTransducerTest, SplitAllHistoryCenterFuture) {
  Init(10, 2, 1, true);
  InitTransducer();
  ContextSet a(num_phones_), b(num_phones_);
  CreatePhoneSets(&a, &b);
  SplitAllModels(-1, &a);
  SplitAllModels(0, &a);
  SplitAllModels(1, &b);
  SplitAllModels(-2, &b);
  SplitAllModels(0, &a);
  SplitAllModels(1, &a);
  SplitAllModels(-1, &b);
  SplitAllModels(-2, &a);
}


TEST_F(ConstructionalTransducerTest, SplitIndividual) {
  Init(10, 2, 1);
  InitTransducer();
  SplitIndividual(1000, 2, false);
}


TEST_F(ConstructionalTransducerTest, SplitIndividualSingleCenter) {
  Init(10, 2, 1, true);
  InitTransducer();
  SplitIndividual(1000, 2, false);
}

TEST_F(ConstructionalTransducerTest, SplitIndividualSharedState) {
  Init(10, 2, 1, true);
  InitSharedStateTransducer();
  SplitIndividual(1000, 2, false);
}

TEST_F(ConstructionalTransducerTest, SplitPrediction) {
  Init(40, 2, 1);
  InitTransducer();
  SplitIndividual(100, 10, true);
}

TEST_F(ConstructionalTransducerTest, SplitPrediction5Phone) {
  Init(40, 3, 1);
  InitTransducer();
  SplitIndividual(100, 10, true);
}

TEST_F(ConstructionalTransducerTest, SplitPredictionSingleCenter) {
  Init(40, 2, 1, true);
  InitTransducer();
  SplitIndividual(100, 10, true);
}

TEST_F(ConstructionalTransducerTest, SplitPredictionSharedState) {
  Init(40, 2, 1, true);
  InitSharedStateTransducer();
  SplitIndividual(100, 10, true);
}


}  // namespace trainc
