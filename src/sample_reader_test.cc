// sample_reader_test.cc
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
// Copyright 2010 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Tests for SampleTextReader

#include "sample_reader.h"
#include "sample.h"
#include "file.h"
#include "fst/symbol-table.h"
#include "stringutil.h"
#include "unittest.h"

namespace trainc {

class SampleTextReaderTest : public ::testing::Test {
public:
  void SetUp() {
    filename_ = FLAGS_test_tmpdir + "/samples.txt";
    symbols_ = new fst::SymbolTable("phones");
  }
  void TearDown() {
    delete symbols_;
  }
  void Init(int dimension, int nsamples,
            int num_left_context, int num_right_context);
  void Test();
protected:
  std::string filename_;
  fst::SymbolTable *symbols_;
  int dimension_, nsamples_, num_left_context_, num_right_context_;
};

void SampleTextReaderTest::Init(int dimension, int nsamples,
                                int num_left_context, int num_right_context) {
  dimension_ = dimension;
  nsamples_ = nsamples;
  num_left_context_ = num_left_context;
  num_right_context_ = num_right_context;

  File *file = File::Create(filename_, "w");
  ASSERT_TRUE(file);
  CHECK(file->Open());
  file->Printf("1 %d %d %d\n", dimension_, num_left_context_,
      num_right_context_);
  for (int s = 0; s < nsamples_; ++s) {
    std::string center = StringPrintf("c%d", s);
    symbols_->AddSymbol(center);
    int state = s % 3;
    file->Printf("%s %d", center.c_str(), state);
    for (int l = 0; l < num_left_context_; ++l) {
      std::string symbol = StringPrintf("l%d", l);
      symbols_->AddSymbol(symbol);
      file->Printf(" %s", symbol.c_str());
    }
    for (int r = 0; r < num_right_context_; ++r) {
      std::string symbol = StringPrintf("r%d", r);
      symbols_->AddSymbol(symbol);
      file->Printf(" %s", symbol.c_str());
    }
    for (int d = 0; d < dimension_ * 2 + 1; ++d) {
      file->Printf(" %f", float((s + 1) * (d + 1)));
    }
    file->Printf("\n");
  }
  file->Close();
  delete file;
}

void SampleTextReaderTest::Test() {
  SampleTextReader reader;
  reader.SetPhoneSymbols(symbols_);
  Samples samples;
  samples.SetNumPhones(symbols_->AvailableKey());
  EXPECT_TRUE(reader.Read(filename_, &samples));
  EXPECT_EQ(dimension_, samples.FeatureDimension());
  for (int s = 0; s < nsamples_; ++s) {
    std::string center = StringPrintf("c%d", s);
    int phone = symbols_->Find(center);
    int state = s % 3;
    const Samples::SampleList &l = samples.GetSamples(phone, state);
    EXPECT_EQ(1, int(l.size()));
    const Sample &sample = l.front();
    EXPECT_EQ(num_left_context_, int(sample.left_context_.size()));
    EXPECT_EQ(num_right_context_, int(sample.right_context_.size()));
    for (int l = 0; l < num_left_context_; ++l) {
      std::string symbol = StringPrintf("l%d", l);
      int cp = symbols_->Find(symbol);
      EXPECT_EQ(sample.left_context_[num_left_context_ - l - 1], cp);
    }
    for (int r = 0; r < num_right_context_; ++r) {
      std::string symbol = StringPrintf("r%d", r);
      int cp = symbols_->Find(symbol);
      EXPECT_EQ(sample.right_context_[r], cp);
    }
    float weight = s + 1;
    EXPECT_EQ(weight, sample.stat.weight());
    int value_base = 2;
    for (int d = 0; d < dimension_; ++d) {
      float value = (s + 1) * value_base++;
      EXPECT_EQ(value, sample.stat.sum()[d]);
    }
    for (int d = 0; d < dimension_; ++d) {
      float value = (s + 1) * value_base++;
      EXPECT_EQ(value, sample.stat.sum2()[d]);
    }
  }
}

TEST_F(SampleTextReaderTest, SingleMonophone) {
  Init(1, 1, 0, 0);
  Test();
}

TEST_F(SampleTextReaderTest, SingleTriphone) {
  Init(1, 1, 1, 1);
  Test();
}

TEST_F(SampleTextReaderTest, Single5Phone) {
  Init(1, 1, 2, 2);
  Test();
}

TEST_F(SampleTextReaderTest, MultiDim) {
  Init(10, 1, 1, 1);
  Test();
}

TEST_F(SampleTextReaderTest, MultiSample) {
  Init(10, 100, 1, 1);
  Test();
}

}  // namespace trainc
