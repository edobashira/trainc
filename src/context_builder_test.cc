// context_builder_test.cc
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
// Tests for ContextBuilder. Create artificial statistics and runs
// the C transducer / model construction.

#include <sstream>
#include "fst/fst-decl.h"
#include "file.h"
#include "context_builder.h"
#include "hmm_compiler.h"
#include "phone_models.h"
#include "phone_sequence.h"
#include "sample.h"
#include "set_inventory.h"
#include "stringutil.h"
#include "transducer_init.h"
#include "unittest.h"
#include "util.h"

namespace trainc {

// Enumerates context dependent phones, generates artificial statistics
// for them, and runs the model splitting.
// The splitting creates a separate model for every context.
// Verifies number of states, number of state models, number of HMMs.
// Executes also all output methods of ContextBuilder.
class ContextBuilderTest : public ::testing::Test {
 public:
  ContextBuilderTest()
      : builder_(NULL),
        num_phones_(-1),
        phone_symbols_(NULL) {}
  virtual void SetUp() {
    builder_ = new ContextBuilder();
  }
  virtual void TearDown();

  virtual void Init(int num_phones,
                    int num_left_contexts, int num_right_contexts,
                    int num_obs, int min_obs, float penalty_weight,
                    float min_gain);
  virtual void RunTest();

 protected:
  typedef list<Sample> SampleList;

  ContextBuilder *builder_;
  static const int kHmmStates;
  static const int kSilenceStates;
  static const int kFeatureDim;

  virtual void Check() const;
  void Write(const string &output_dir) const;

  void CreatePhones();
  virtual void CreateQuestions();
  void SetParameters();
  void CreateStatistics();
  virtual void CreateMapping() {}
  virtual void CreateStatisticsForState(int phone, int state,
                                        Samples *samples);

  int num_phones_, min_obs_, num_obs_, penalty_weight_;
  int num_left_contexts_, num_right_contexts_;
  float min_gain_;
  fst::SymbolTable *phone_symbols_;
  set<int> ci_phones_;
  int silence_phone_;
  SetInventory questions_;
};

const int ContextBuilderTest::kHmmStates = 3;
const int ContextBuilderTest::kSilenceStates = 1;
const int ContextBuilderTest::kFeatureDim = 1;

void ContextBuilderTest::TearDown() {
  delete builder_;
  delete phone_symbols_;
  builder_ = NULL;
  phone_symbols_ = NULL;
}

void ContextBuilderTest::Init(
    int num_phones, int num_left_contexts, int num_right_contexts,
    int num_obs, int min_obs, float penalty_weight, float min_gain) {
  num_phones_ = num_phones;
  num_obs_ = num_obs;
  min_obs_ = min_obs;
  min_gain_ = min_gain;
  penalty_weight_ = penalty_weight;
  num_left_contexts_ = num_left_contexts;
  num_right_contexts_ = num_right_contexts;
  builder_->SetContextLength(num_left_contexts, num_right_contexts, false);
  CreatePhones();
  ci_phones_.clear();
  ci_phones_.insert(silence_phone_);
  builder_->SetCiPhones(ci_phones_);
  builder_->SetBoundaryPhone("sil");
  CreateMapping();
  CreateQuestions();


  CreateStatistics();
  SetParameters();
}

void ContextBuilderTest::RunTest() {
  builder_->Build();
  Write(FLAGS_test_tmpdir);
  EXPECT_TRUE(builder_->CheckTransducer());
  Check();
}

void ContextBuilderTest::SetParameters() {
  builder_->SetMinSplitGain(min_gain_);
  builder_->SetMinSeenContexts(0);
  builder_->SetMinObservations(min_obs_);
  builder_->SetVarianceFloor(0.0001);
  builder_->SetTargetNumModels(0);
  builder_->SetStatePenaltyWeight(penalty_weight_);
}

void ContextBuilderTest::CreatePhones() {
  delete phone_symbols_;
  phone_symbols_ = new fst::SymbolTable("phones");
  ASSERT_EQ(phone_symbols_->AddSymbol("eps", 0), 0);
  for (int p = 1; p < num_phones_; ++p) {
    string symbol = StringPrintf("p%d", p);
    phone_symbols_->AddSymbol(symbol);
  }
  silence_phone_ = phone_symbols_->AddSymbol("sil");
  builder_->SetPhoneSymbols(*phone_symbols_);
  for (int p = 1; p < num_phones_; ++p)
    builder_->SetPhoneLength(p, kHmmStates);
  builder_->SetPhoneLength(silence_phone_, kSilenceStates);
  VLOG(2) << "# phones: " << phone_symbols_->NumSymbols();
}

void ContextBuilderTest::CreateQuestions() {
  SetInventory questions;
  const string question_file = FLAGS_test_tmpdir + "/questions.txt";
  File *file = File::OpenOrDie(question_file, "w");
  for (int p = 1; p <= num_phones_; ++p) {
    if (p == silence_phone_) continue;
    string symbol = phone_symbols_->Find(p);
    file->Printf("%s %s\n", symbol.c_str(), symbol.c_str());
  }
  file->Close();
  delete file;
  questions.SetSymTable(*phone_symbols_);
  questions.ReadText(question_file);
  VLOG(1) << "generated questions: " << questions.NumSets();
  builder_->SetDefaultQuestionSet(questions);
}

void ContextBuilderTest::CreateStatistics() {
  Samples *samples = new Samples();
  samples->SetNumPhones(num_phones_ + 1);
  samples->SetFeatureDimension(kFeatureDim);
  for (int p = 1; p <= num_phones_; ++p) {
    const int num_states = p < num_phones_ ? kHmmStates : kSilenceStates;
    for (int s = 0; s < num_states; ++s) {
      CreateStatisticsForState(p, s, samples);
    }
  }
  builder_->SetSamples(samples);
}

void ContextBuilderTest::CreateStatisticsForState(
    int phone, int state, Samples *samples) {
  const int length = num_left_contexts_ + num_right_contexts_;
  PhoneSequenceIterator allophones(length, phone_symbols_);
  const vector<float> observation(kFeatureDim, 1.0);
  int index = 0;
  while (!allophones.Done()) {
    Sample *sample = samples->AddSample(phone, state);
    sample->left_context_.resize(num_left_contexts_, -1);
    sample->right_context_.resize(num_right_contexts_, -1);
    for (int o = 0; o < num_obs_; ++o)
      sample->stat.AddObservation(observation);
    vector<int> context;
    allophones.IndexValue(&context);
    for (int i = 0; i < length; ++i) {
      int &c = (i < num_left_contexts_ ?
          sample->left_context_[i] : sample->right_context_[i - num_left_contexts_]);
      c = context[i];
    }
    allophones.Next();
    ++index;
  }
  VLOG(1) << "statistics for phone=" << phone_symbols_->Find(phone)
          << " state=" << state << " n=" << index;
}

void ContextBuilderTest::Check() const {
  int expect_states = num_phones_ - 1;
  int silence_states = 1;
  int expect_models = kHmmStates * (num_phones_ - 1);
  int expect_hmms = num_phones_ - 1;
  for (int i = 0; i < num_left_contexts_; ++i) {
    expect_states *= num_phones_;
    if (i > 0) silence_states *= num_phones_;
    expect_models *= num_phones_;
    expect_hmms *= num_phones_;
  }
  for (int i = 0; i < num_right_contexts_; ++i) {
    expect_models *= num_phones_;
    expect_hmms *= num_phones_;
  }
  // for silence
  expect_states += silence_states;
  expect_models += kSilenceStates;
  expect_hmms += 1;
  EXPECT_EQ(builder_->NumStates(), expect_states);
  EXPECT_EQ(builder_->GetHmmCompiler().NumStateModels(), expect_models);
  EXPECT_EQ(builder_->GetHmmCompiler().NumHmmModels(), expect_hmms);
}

void ContextBuilderTest::Write(const string &output_dir) const {
  const HmmCompiler &hc = builder_->GetHmmCompiler();
  hc.WriteHmmSymbols(output_dir + "/hmm.sym");
  hc.WriteCDHMMtoPhoneMap(output_dir + "/phone_map");
  hc.WriteHmmList(output_dir + "/hmm_list");
  hc.WriteHmmSymbols(output_dir + "/hmm_symbols");
  hc.WriteHmmTransducer(output_dir + "/h_transducer");
  hc.WriteStateModelInfo(output_dir + "/state_models.log");
  hc.WriteStateModels(output_dir + "/state_models", "text", "test", "test");
  hc.WriteStateNameMap(output_dir + "/state_map");
  hc.WriteStateSymbols(output_dir + "/state_symbols");
  builder_->WriteTransducer(output_dir + "/c.fst");
  builder_->WriteStateInfo(output_dir + "/states.log");
}

// ===========================================================

class ContextBuilderMappedTest : public ContextBuilderTest {
 public:
  virtual void RunTest();
 protected:
  void Check() const;
  virtual void CreateMapping();
  virtual const string InitType() const {
    return TransducerInitializationFactory::kTiedModel;
  }
};

void ContextBuilderMappedTest::RunTest() {
  builder_->Build();
  Write(FLAGS_test_tmpdir);
  Check();
}

void ContextBuilderMappedTest::CreateMapping() {
  const string map_file = FLAGS_test_tmpdir + "/map.txt";
  File *file = File::OpenOrDie(map_file, "w");
  ASSERT_GT(num_phones_, 5);
  for (int p = 1; p <= 3; p += 2) {
    const string &src = phone_symbols_->Find(p);
    const string &dst = phone_symbols_->Find(p + 1);
    file->Printf("%s %s\n", src.c_str(),
                            dst.c_str());
    VLOG(2) << "map " << src << " to " << dst;
  }
  file->Close();
  builder_->SetTransducerInitType(InitType());
  builder_->SetPhoneMapping(map_file);
  if (InitType() == TransducerInitializationFactory::kSharedState)
    builder_->SetContextLength(num_left_contexts_, num_right_contexts_, true);
}

void ContextBuilderMappedTest::Check() const {
  int expect_states = num_phones_ - 1;
  int silence_states = 1;
  int expect_models = kHmmStates * (num_phones_ - 3);
  int expect_hmms = num_phones_ - 3;
  for (int i = 0; i < num_left_contexts_; ++i) {
    expect_states *= num_phones_;
    if (i > 0) silence_states *= num_phones_;
    expect_models *= num_phones_;
    expect_hmms *= num_phones_;
  }
  for (int i = 0; i < num_right_contexts_; ++i) {
    expect_models *= num_phones_;
    expect_hmms *= num_phones_;
  }
  // for silence
  expect_states += silence_states;
  expect_models += kSilenceStates;
  expect_hmms += 1;
  EXPECT_EQ(builder_->NumStates(), expect_states);
  EXPECT_EQ(builder_->GetHmmCompiler().NumStateModels(), expect_models);
  EXPECT_EQ(builder_->GetHmmCompiler().NumHmmModels(), expect_hmms);
}

// ===========================================================

class ContextBuilderMappedStateTest : public ContextBuilderMappedTest {
 public:
  ContextBuilderMappedStateTest() : center_questions_(false) {}

  void SetCenterQuestions(bool cq) {
    center_questions_ = cq;
  }

 protected:
  void Check() const;
  virtual const string InitType() const {
    return TransducerInitializationFactory::kSharedState;
  }
  void CreateQuestions();
  bool center_questions_;
};

void ContextBuilderMappedStateTest::Check() const {
  if (!center_questions_) {
    ContextBuilderTest::Check();
  } else {
    int expect_states = num_phones_ - 1;
    int silence_states = 1;
    int expect_models = kHmmStates * (num_phones_ - 2);
    int expect_hmms = num_phones_ - 2;
    for (int i = 0; i < num_left_contexts_; ++i) {
      expect_states *= num_phones_;
      if (i > 0) silence_states *= num_phones_;
      expect_models *= num_phones_;
      expect_hmms *= num_phones_;
    }
    for (int i = 0; i < num_right_contexts_; ++i) {
      expect_models *= num_phones_;
      expect_hmms *= num_phones_;
    }
    // for silence
    expect_states += silence_states;
    expect_models += kSilenceStates;
    expect_hmms += 1;
    EXPECT_EQ(builder_->NumStates(), expect_states);
    EXPECT_EQ(builder_->GetHmmCompiler().NumStateModels(), expect_models);
    EXPECT_EQ(builder_->GetHmmCompiler().NumHmmModels(), expect_hmms);
  }
}

void ContextBuilderMappedStateTest::CreateQuestions() {
  ContextBuilderTest::CreateQuestions();
  if (center_questions_) {
    SetInventory questions;
    const string question_file = FLAGS_test_tmpdir + "/center_questions.txt";
    File *file = File::OpenOrDie(question_file, "w");
    file->Printf("p3 p3\n");
    file->Printf("p4 p4\n");
    file->Close();
    questions.SetSymTable(*phone_symbols_);
    questions.ReadText(question_file);
    VLOG(1) << "generated questions: " << questions.NumSets();
    builder_->SetQuestionSetPerContext(0, questions);
  }
}

// ===========================================================

class ContextBuilderModelTest : public ContextBuilderTest {
 public:
  virtual void Init(int num_phones,
                    int num_left_contexts, int num_right_contexts,
                    int num_obs, int min_obs, float penalty_weight,
                    float min_gain);

 protected:
  virtual void Check() const;
  virtual void CreateStatisticsForState(int phone, int state,
                                        Samples *samples);
  void CheckContextSets(const AllophoneStateModel &state_model,
                        int phone, int state) const;
 private:
  vector< vector<int> > model_order_;
};

void ContextBuilderModelTest::Init(
    int num_phones, int num_left_contexts, int num_right_contexts,
    int num_obs, int min_obs, float penalty_weight, float min_gain) {
  model_order_.resize(num_phones + 1);
  ContextBuilderTest::Init(num_phones, num_left_contexts, num_right_contexts,
                           num_obs, min_obs, penalty_weight, min_gain);
}

void ContextBuilderModelTest::CreateStatisticsForState(
    int phone, int state, Samples *samples) {
  const int length = num_left_contexts_ + num_right_contexts_;
  PhoneSequenceIterator allophones(length, phone_symbols_);


  int index = 0;
  // select the order of this phone, with the following meaning
  //  0 = context indepepent
  //  1 = dep on all context
  //  2 = dep. on right context
  //  3 = dep. on left context -1
  //  4 = dep. on left context -2
  //  ...
  int order = (phone * kHmmStates + state) % (length + 2);
  VLOG(2) << "phone = " << phone;
  ASSERT_EQ(model_order_[phone].size(), state);
  model_order_[phone].push_back(order);
  VLOG(2) << phone_symbols_->Find(phone) << " " << state << " order="<< order;
  while (!allophones.Done()) {
    Sample *sample = samples->AddSample(phone, state);
    sample->left_context_.resize(num_left_contexts_, -1);
    sample->right_context_.resize(num_right_contexts_, -1);

    vector<int> context;
    allophones.IndexValue(&context);
    std::stringstream str;
    // the feature groups equivalent contexts of this hmm state, i.e.
    // all contexts which should not be distinguished get the same feature.
    // if order == 0, all allophones are assigned to feature 1
    // if order == 1, all allophones with the same right context
    //   are assigned to the same feature, etc.
    int feature = 1;
    for (int pos = 0; pos < length; ++pos) {
      int &c = (pos < num_left_contexts_ ?
          sample->left_context_[num_left_contexts_ - (pos + 1)] :
          sample->right_context_[pos - num_left_contexts_]);
      c = context[pos];
      str << phone_symbols_->Find(context[pos]) << " ";
      if ((order == 1) || ((pos == length -1 ) && (order == 2)) ||
          ((order >= 3) && (pos == (num_left_contexts_ - order + 2))))
        feature = feature * num_phones_ + c;
    }
    VLOG(2) << phone_symbols_->Find(phone) << " context: " << str.str();
    VLOG(2) << "feature= " << feature;
    const vector<float> observation(kFeatureDim, feature);
    sample->stat.AddObservation(observation);
    ++index;
    allophones.Next();
  }
  VLOG(1) << "statistics for phone=" << phone_symbols_->Find(phone)
          << " state=" << state  << " " << index;
}

void ContextBuilderModelTest::Check() const {
  typedef vector<const AllophoneStateModel*>::const_iterator ModelIter;
  const HmmCompiler &hc = builder_->GetHmmCompiler();
  for (int p = 1; p <= num_phones_; ++p) {
    const int num_states = p < num_phones_ ? kHmmStates : kSilenceStates;
    for (int s = 0; s < num_states; ++s) {
      vector<const AllophoneStateModel*> models = hc.GetStateModels(p, s);
      for (ModelIter model = models.begin(); model != models.end(); ++model) {
        CheckContextSets(*(*model), p, s);
      }
    }
  }
}

void ContextBuilderModelTest::CheckContextSets(
    const AllophoneStateModel &state_model, int phone, int state) const {
  const PhoneContext &context = state_model.GetContext();
  const int order = model_order_[phone][state];
  for (int c = -num_left_contexts_; c <= num_right_contexts_; ++c) {
    if (!c) continue;
    bool is_cd = (order == 1 || (order == 2 && c > 0) ||
                  (order - 3 == -(c + 1)));
    const ContextSet &set = context.GetContext(c);
    int set_size = 0;
    for (int p = 0; p < num_phones_; ++p) {
      set_size += set.HasElement(p);
    }
    if (phone == silence_phone_)
      // silence has empty context
      EXPECT_EQ(set_size, 0);
    else if (is_cd)
      // separate model for each context phone at this position
      EXPECT_EQ(set_size, 1);
    else
      // set of all phones
      EXPECT_EQ(set_size, num_phones_);
  }
}

TEST_F(ContextBuilderTest, Triphones) {
  const int num_phones = 4;
  const int left_context = 1;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderTest, Quadrophones) {
  const int num_phones = 4;
  const int left_context = 2;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderModelTest, Triphones) {
  const int num_phones = 4;
  const int left_context = 1;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 0;
  const float min_gain = 0.0001;
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderModelTest, Quadrophones) {
  const int num_phones = 4;
  const int left_context = 2;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 0;
  const float min_gain = 0.0001;
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderMappedTest, Triphones) {
  const int num_phones = 6;
  const int left_context = 1;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderMappedTest, Quadrophones) {
  const int num_phones = 6;
  const int left_context = 2;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderMappedStateTest, Triphones) {
  const int num_phones = 6;
  const int left_context = 1;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  const bool center_questions = false;
  SetCenterQuestions(center_questions);
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderMappedStateTest, TriphonesCenterQuestion) {
  const int num_phones = 6;
  const int left_context = 1;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  const bool center_questions = true;
  SetCenterQuestions(center_questions);
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

TEST_F(ContextBuilderTest, ReplayTest) {
  std::string file = FLAGS_test_tmpdir + "/splits";
  const int num_phones = 4;
  const int left_context = 1;
  const int right_context = 1;
  const int num_obs = 1;
  const int min_obs = 1;
  const int state_penalty = 100000;
  const float min_gain = 0.0;
  builder_->SetSaveSplits(file);
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
  TearDown();
  SetUp();
  builder_->SetReplay(file);
  Init(num_phones, left_context, right_context,
       num_obs, min_obs, state_penalty, min_gain);
  RunTest();
}

}  // namespace trainc
