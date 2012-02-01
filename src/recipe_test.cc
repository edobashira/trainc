// recipe_test.cc
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
//
// \file
// Test cases for RecipeWriter and RecipeReader

#include "recipe.h"
#include "unittest.h"
#include "model_splitter.h"

namespace trainc {

class RecipeTest : public ::testing::Test {
public:
  void SetUp();
  void TearDown();
protected:
  AllophoneStateModel *a_;
  ContextSet *cl_, *cr_;
  PhoneContext *pc_;
  std::string filename_;
  static const int num_phones;
  static const int hmm_state;
  static const int pl1;
  static const int pl2;
  static const int pr;
};

const int RecipeTest::num_phones = 10;
const int RecipeTest::hmm_state = 2;
const int RecipeTest::pl1 = 2;
const int RecipeTest::pl2 = 4;
const int RecipeTest::pr = 3;


void RecipeTest::SetUp() {
  pc_ = new PhoneContext(num_phones, 1, 1);
  cl_ = new ContextSet(num_phones);
  cl_->Add(pl1);
  cl_->Add(pl2);
  cr_ = new ContextSet(num_phones);
  cr_->Add(pr);
  pc_->SetContext(-1, *cl_);
  pc_->SetContext(1, *cr_);
  a_ = new AllophoneStateModel(hmm_state, *pc_);
  filename_ = ::tempnam(NULL, NULL);
}

void RecipeTest::TearDown() {
  delete a_;
  delete cl_;
  delete cr_;
  delete pc_;
}

TEST_F(RecipeTest, IsEqual) {
  const int num_states = 1;
  AllophoneModel am1(5, num_states);
  AllophoneModel am2(6, num_states);
  a_->AddAllophoneRef(&am1);
  a_->AddAllophoneRef(&am2);

  AllophoneStateModel *a2 = a_->Clone();
  a2->GetContextRef()->GetContextRef(-1)->Remove(pl1);

  EXPECT_TRUE(AllophoneModelStub(am1).IsEqual(am1));
  EXPECT_FALSE(AllophoneModelStub(am1).IsEqual(am2));
  EXPECT_FALSE(AllophoneModelStub(am2).IsEqual(am1));
  EXPECT_TRUE(AllophoneStateModelStub(*a_).IsEqual(*a_));
  EXPECT_FALSE(AllophoneStateModelStub(*a_).IsEqual(*a2));
  EXPECT_FALSE(AllophoneStateModelStub(*a2).IsEqual(*a_));
  delete a2;
}

TEST_F(RecipeTest, Add) {
  const int num_states = 1;
  AllophoneModel am1(5, num_states);
  AllophoneModel am2(6, num_states);
  a_->AddAllophoneRef(&am1);
  a_->AddAllophoneRef(&am2);

  ModelManager::StateModelList models;
  models.push_back(a_);
  ModelManager::StateModelRef model_ref = models.begin();

  ContextQuestion q1(ContextSet(0), "q1"), q2(ContextSet(0), "q2");
  std::vector<const ContextBuilder::QuestionSet*> questions;
  ContextBuilder::QuestionSet question_set;
  question_set.push_back(&q1);
  question_set.push_back(&q2);
  for (int i = 0; i < 3; ++i) {
    questions.push_back(&question_set);
  }
  SplitHypothesis split;
  split.model = model_ref;
  split.question = &q2;
  split.position = 1;

  {
    File *file = File::Create(filename_, "w");
    RecipeWriter writer(file);
    writer.Init();
    writer.SetQuestions(1, &questions);
    writer.AddSplit(split);
  }
  SplitDef def;
  {
    File *file = File::Create(filename_, "r");
    RecipeReader reader(file);
    EXPECT_TRUE(reader.Init());
    EXPECT_TRUE(reader.ReadSplit(&def));
    SplitDef def2;
    EXPECT_FALSE(reader.ReadSplit(&def2));
  }

  EXPECT_EQ(split.position, def.position);
  EXPECT_EQ(1, def.question);
  EXPECT_TRUE(def.model.IsEqual(*a_));
}


}  // namespace trainc {



