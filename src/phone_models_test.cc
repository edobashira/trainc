// phone_models_test.cc
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
// Tests for the classes PhoneContext, AllophoneStateModel, AllphoneModel,
// Phones. Check basic functionality. No tests for methods that depend on
// AllphoneStateModel::Data.

#include <ext/numeric>
#include "unittest.h"
#include "util.h"
#include "phone_models.h"

using __gnu_cxx::iota;

namespace trainc {

class AllophoneStateModelTest : public ::testing::Test {
 protected:
  AllophoneStateModelTest() {}

  virtual void SetUp();
  virtual void TearDown();

  AllophoneStateModel *a_;
  ContextSet *cl_, *cr_;
  PhoneContext *pc_;
  static const int num_phones;
  static const int hmm_state;
  static const int pl1;
  static const int pl2;
  static const int pr;
};

const int AllophoneStateModelTest::num_phones = 10;
const int AllophoneStateModelTest::hmm_state = 2;
const int AllophoneStateModelTest::pl1 = 2;
const int AllophoneStateModelTest::pl2 = 4;
const int AllophoneStateModelTest::pr = 3;


void AllophoneStateModelTest::SetUp() {
  pc_ = new PhoneContext(num_phones, 1, 1);
  cl_ = new ContextSet(num_phones);
  cl_->Add(pl1);
  cl_->Add(pl2);
  cr_ = new ContextSet(num_phones);
  cr_->Add(pr);
  pc_->SetContext(-1, *cl_);
  pc_->SetContext(1, *cr_);
  a_ = new AllophoneStateModel(hmm_state, *pc_);
}

void AllophoneStateModelTest::TearDown() {
  delete a_;
  delete cl_;
  delete cr_;
  delete pc_;
}

TEST_F(AllophoneStateModelTest, Accessor) {
  const int state = hmm_state;
  EXPECT_EQ(state, a_->state());
  EXPECT_TRUE(a_->context(-1).IsEqual(*cl_));
  EXPECT_TRUE(a_->context(1).IsEqual(*cr_));
}

TEST_F(AllophoneStateModelTest, AllophoneRef) {
  const int phone = 9;
  const int num_states = 3;
  AllophoneModel am(phone, num_states);
  a_->AddAllophoneRef(&am);
  EXPECT_EQ(a_->GetAllophones().front(), &am);
  a_->RemoveAllophoneRef(&am);
  EXPECT_TRUE(a_->GetAllophones().empty());
}

TEST_F(AllophoneStateModelTest, Clone) {
  const int phone = 9;
  const int num_states = 3;
  AllophoneModel am(phone, num_states);
  a_->AddAllophoneRef(&am);
  AllophoneStateModel *clone = a_->Clone();
  EXPECT_EQ(a_->state(), clone->state());
  EXPECT_TRUE(a_->context(-1).IsEqual(clone->context(-1)));
  EXPECT_TRUE(a_->context(1).IsEqual(clone->context(1)));
  EXPECT_TRUE(clone->GetAllophones().empty());
  delete clone;
}

TEST_F(AllophoneStateModelTest, Split) {
  const int phone1 = 9;
  const int phone2 = 8;
  const int num_states = 3;
  AllophoneModel am1(phone1, num_states);
  AllophoneModel am2(phone2, num_states);
  a_->AddAllophoneRef(&am1);
  a_->AddAllophoneRef(&am2);
  ContextSet qc(num_phones);
  qc.Add(pl1);
  ContextQuestion q(qc);
  AllophoneStateModel::SplitResult s = a_->Split(-1, q);
  ASSERT_TRUE(s.first != NULL);
  ASSERT_TRUE(s.second != NULL);
  EXPECT_TRUE(s.first->context(-1).HasElement(pl1));
  EXPECT_FALSE(s.first->context(-1).HasElement(pl2));
  EXPECT_TRUE(s.second->context(-1).HasElement(pl2));
  EXPECT_FALSE(s.second->context(-1).HasElement(pl1));
  EXPECT_EQ(a_->state(), s.first->state());
  EXPECT_EQ(a_->state(), s.second->state());
  EXPECT_TRUE(s.first->context(1).IsEqual(a_->context(1)));
  EXPECT_TRUE(s.second->context(1).IsEqual(a_->context(1)));
  delete s.first;
  delete s.second;
}

TEST_F(AllophoneStateModelTest, SplitAllophones) {
  const int phone_a = 2;
  const int phone_b = 4;
  const int num_states = 3;
  vector<AllophoneStateModel*> models;
  AllophoneModel m_a(phone_a, num_states), m_b(phone_b, num_states);
  for (int a = 0; a < 2; ++a) {
    for (int s = 0; s < num_states; ++s) {
      AllophoneStateModel *sm;
      if (s != hmm_state) {
        sm = new AllophoneStateModel(s, *pc_);
        models.push_back(sm);
      } else {
        sm = a_;
      }
      AllophoneModel &al = (a ? m_b : m_a);
      al.SetStateModel(s, sm);
      sm->AddAllophoneRef(&al);
    }
  }
  ContextSet qc(num_phones);
  qc.Add(pl1);
  ContextQuestion q(qc);
  AllophoneStateModel::SplitResult s = a_->Split(-1, q);
  ModelSplit split;
  a_->SplitAllophones(-1, s, &split);
  EXPECT_EQ((size_t)2, split.phone_models.size());
  EXPECT_EQ(&m_a, split.phone_models[1].old_model);
  EXPECT_EQ(&m_b, split.phone_models[0].old_model);
  for (int s = 0; s < num_states; ++s) {
    if (s == hmm_state) continue;
    EXPECT_EQ(m_a.GetStateModel(s),
        split.phone_models[1].new_models.first->GetStateModel(s));
    EXPECT_EQ(m_a.GetStateModel(s),
        split.phone_models[1].new_models.second->GetStateModel(s));
    EXPECT_EQ(m_b.GetStateModel(s),
        split.phone_models[0].new_models.first->GetStateModel(s));
    EXPECT_EQ(m_b.GetStateModel(s),
        split.phone_models[0].new_models.second->GetStateModel(s));
  }
  delete s.first;
  delete s.second;
  STLDeleteElements(&models);
  std::vector<AllophoneModelSplit>::iterator i = split.phone_models.begin();
  for (; i != split.phone_models.end(); ++i) {
    delete i->new_models.first;
    delete i->new_models.second;
  }
}


class PhonesTest : public ::testing::Test {
 protected:
  PhonesTest() {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  static const int num_phones;
};

const int PhonesTest::num_phones = 10;


TEST_F(PhonesTest, Test) {
  Phones pl(num_phones);
  EXPECT_EQ(pl.NumPhones(), num_phones);
  pl.SetPhoneLength(0, 1);
  pl.SetPhoneLength(num_phones - 1, 3);
  EXPECT_EQ(pl.NumHmmStates(0), 1);
  EXPECT_EQ(pl.NumHmmStates(num_phones - 1), 3);
  for (int p = 1; p < num_phones - 1; ++p)
    EXPECT_EQ(pl.NumHmmStates(p), -1);

  Phones pv(num_phones);
  vector<int> lengths(num_phones, 0);
  iota(lengths.begin(), lengths.end(), 5);
  pv.SetPhoneLenghts(lengths);
  for (int p = 0; p < num_phones; ++p)
    EXPECT_EQ(pv.NumHmmStates(p), lengths[p]);

  Phones pc(num_phones);
  pc.SetCiPhone(0);
  pc.SetCiPhone(num_phones - 1);
  for (int p = 0; p < num_phones; ++p) {
    if (p == 0 || p == num_phones - 1)
      EXPECT_TRUE(pc.IsCiPhone(p));
    else
      EXPECT_FALSE(pc.IsCiPhone(p));
  }
}

}  // namespace trainc
