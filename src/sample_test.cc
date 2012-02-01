// sample_test.cc
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
// Unit tests for Sample, Statistics, Samples, Scorer

#include "sample.h"
#include "scorer.h"
#include "unittest.h"

namespace trainc {

class StatisticsTest : public ::testing::Test {
public:
  void Init(int dim) {
    dim_ = dim;
  }

  void TestCreate() {
    Statistics stat(dim_);
    EXPECT_EQ(stat.dimension(), dim_);
  }

  void TestMembers() {
    Statistics stat(dim_);
    const float weight = dim_ * 3;
    stat.SetWeight(weight);
    for (int d = 0; d < dim_; ++d) {
      stat.SumRef()[d] = d;
      stat.Sum2Ref()[d] = d * d;
    }
    EXPECT_EQ(weight, stat.weight());
    for (int d = 0; d < dim_; ++d) {
      EXPECT_EQ(stat.sum()[d], float(d));
      EXPECT_EQ(stat.sum2()[d], float(d * d));
    }
  }

  void TestAccumulate() {
    Statistics a(dim_), b(dim_);
    float weight_a = 1, weight_b = 2;
    a.SetWeight(weight_a);
    b.SetWeight(weight_b);
    for (int d = 0; d < dim_; ++d) {
      a.SumRef()[d] = 1;
      b.SumRef()[d] = d;
      a.Sum2Ref()[d] = 2;
      b.Sum2Ref()[d] = d;
    }
    a.Accumulate(b);
    EXPECT_EQ(dim_, a.dimension());
    EXPECT_EQ(weight_a + weight_b, a.weight());
    for (int d = 0; d < dim_; ++d) {
      EXPECT_EQ(float(d + 1), a.sum()[d]);
      EXPECT_EQ(float(d + 2), a.sum2()[d]);
    }
  }

  void TestAll() {
    TestCreate();
    TestMembers();
    TestAccumulate();
  }

protected:
  int dim_;
};

TEST_F(StatisticsTest, Size1) {
  Init(1);
  TestAll();
}

TEST_F(StatisticsTest, Size45) {
  Init(45);
  TestAll();
}


TEST(Samples, Create) {
  const int num_phones = 10;
  const int num_states = 3;
  const int max_samples = 100;
  const int num_left_ctxt = 2;
  const int num_right_ctxt = 1;
  const int dim = 1;
  Samples samples;
  samples.SetFeatureDimension(dim);
  samples.SetNumPhones(num_phones);
  for (int p = 0; p < num_phones; ++p) {
    for (int s = (p % 2); s < num_states * 2; s += 2) {
      const int num_samples = ((p + 1) * (s + 1)) % max_samples;
      for (int i = 0; i < num_samples; ++i) {
        Sample *sample = samples.AddSample(p, s);
        sample->left_context_.resize(num_left_ctxt);
        sample->right_context_.resize(num_right_ctxt);
        for (int l = 0; l < num_left_ctxt; ++l)
          sample->left_context_[l] = p + l + 1;
        for (int r = 0; r < num_right_ctxt; ++r)
          sample->right_context_[r] = p + r + 2;
        Statistics &stat = sample->stat;
        EXPECT_EQ(dim, stat.dimension());
        for (int d = 0; d < dim; ++d)
          stat.SumRef()[d] = p + s;
      }
    }
  }

  for (int p = 0; p < num_phones; ++p) {
    for (int s = 0; s < num_states * 2 - (p + 1) % 2; ++s) {
      const Samples::SampleList &sample_list = samples.GetSamples(p, s);
      if ((s % 2) != (p % 2)) {
        EXPECT_EQ(0, int(sample_list.size()));
        continue;
      }
      const int num_samples = ((p + 1) * (s + 1)) % max_samples;
      EXPECT_EQ(num_samples, int(sample_list.size()));
      Samples::SampleList::const_iterator sample_it = sample_list.begin();
      for (int i = 0; i < num_samples; ++i, ++sample_it) {
        const Sample &sample = *sample_it;
        EXPECT_EQ(num_left_ctxt, int(sample.left_context_.size()));
        EXPECT_EQ(num_right_ctxt, int(sample.right_context_.size()));
        for (int l = 0; l < num_left_ctxt; ++l)
          EXPECT_EQ(p + l + 1, sample.left_context_[l]);
        for (int r = 0; r < num_right_ctxt; ++r)
          EXPECT_EQ(p + r + 2, sample.right_context_[r]);
        const Statistics &stat = sample.stat;
        for (int d = 0; d < dim; ++d)
          EXPECT_EQ(float(p + s), stat.sum()[d]);
      }
    }
  }
}

TEST(Scorer, Score) {
  std::list<Sample> samples;
  const int num_samples = 3;
  const int dimension = 2;
  const float result = 7.2972358749035449;
  Statistics sum(dimension);
  for (int s = 0; s < num_samples; ++s) {
    samples.push_back(Sample(dimension));
    Sample &sample = samples.back();
    float v = s + 1;
    std::fill(sample.stat.SumRef(), sample.stat.SumRef() + dimension, float(v));
    std::fill(sample.stat.Sum2Ref(), sample.stat.Sum2Ref() + dimension, float(v * v));
    sample.stat.SetWeight(1.0);
    sum.Accumulate(sample.stat);
  }
  MaximumLikelihoodScorer scorer(0.1);
  float score = scorer.score(sum);
  EXPECT_LT(fabs(score - result), 1e-6);
}

}  // namespace trainc
