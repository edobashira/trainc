// gaussian_model_test.cc
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
// Tests for GaussianModel an ModelWriter

#include <algorithm>
#include <functional>
#include "gaussian_model.h"
#include "sample.h"
#include "unittest.h"

namespace trainc {

class GaussianModelTest : public ::testing::Test {
public:
  GaussianModelTest() : model_(NULL) {}
  virtual void SetUp();
  virtual void TearDown() { delete model_; }
protected:
  GaussianModel *model_;
  int dim_, num_models_;
  float mean_, mean_mult_;
  float cov_, cov_mult_;
  std::vector<std::string> names_;
};

void GaussianModelTest::SetUp() {
  model_ = new GaussianModel();
  dim_ = 10;
  num_models_ = 3;
  mean_ = 2;
  cov_ = 1;
  mean_mult_ = 2;
  cov_mult_ = 3;
  std::vector<float> mean(dim_, mean_);
  std::vector<float> cov(dim_, cov_);
  for (int m = 0; m < num_models_; ++m) {
    names_.push_back(std::string(1, 'A' + m));
    model_->AddModel(names_[m], mean, cov);
    std::transform(mean.begin(), mean.end(), mean.begin(),
                   std::bind2nd(std::multiplies<float>(), mean_mult_));
    std::transform(cov.begin(), cov.end(), cov.begin(),
                   std::bind2nd(std::multiplies<float>(), cov_mult_));
  }
}

TEST_F(GaussianModelTest, AddModel) {
  EXPECT_EQ(num_models_, model_->NumModels());
  EXPECT_EQ(dim_, model_->Dimension());
  float mean = mean_;
  float cov = cov_;
  for (int i = 0; i < num_models_; ++i)
    EXPECT_EQ(i, model_->GetIndex(names_[i]));
  int i = 0;
  for (GaussianModel::Iterator m = model_->GetIterator(); !m.Done(); m.Next(), ++i) {
    EXPECT_EQ(names_[i], m.Name());
    EXPECT_EQ(i, int(m.Index()));

    const std::vector<float> &mm = model_->Mean(m.Index());
    const std::vector<float> &mv = model_->Variance(m.Index());
    for (int d = 0; d < dim_; ++d) {
      EXPECT_EQ(mean, mm[d]);
      EXPECT_EQ(cov, mv[d]);
    }
    mean *= mean_mult_;
    cov *= cov_mult_;
  }
}

TEST_F(GaussianModelTest, Estimate) {
  GaussianModel model;
  Statistics stat(dim_);
  int num_obs = 4;
  stat.SetWeight(4);
  const std::string name = "test";
  std::fill(stat.SumRef(), stat.SumRef() + dim_, mean_ * num_obs);
  std::fill(stat.Sum2Ref(), stat.Sum2Ref() + dim_, (cov_ + mean_ * mean_) * num_obs);
  model.Estimate(name, stat, 0);
  EXPECT_EQ(1, model.NumModels());
  EXPECT_EQ(dim_, model.Dimension());
  EXPECT_EQ(0, model.GetIndex(name));
  const std::vector<float> &mm = model_->Mean(0);
  const std::vector<float> &mv = model_->Variance(0);
  for (int d = 0; d < dim_; ++d) {
    EXPECT_EQ(mean_, mm[d]);
    EXPECT_EQ(cov_, mv[d]);
  }
}

TEST_F(GaussianModelTest, WriteText) {
  ModelTextWriter writer;
  bool r = writer.Write(FLAGS_test_tmpdir + "/model", *model_);
  EXPECT_TRUE(r);
}

TEST_F(GaussianModelTest, WriteRwthText) {
  RwthModelTextWriter writer;
  bool r = writer.Write(FLAGS_test_tmpdir + "/rwthmodel", *model_);
  EXPECT_TRUE(r);
}


}  // namespace trainc
