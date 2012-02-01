// sample.cc
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

#include "sample.h"

namespace trainc {

Samples::Samples()
  : feature_dim_(-1) {}

void Samples::SetNumPhones(int num_phones) {
  samples_.resize(num_phones);
}

int Samples::NumPhones() const {
  return samples_.size();
}

void Samples::SetFeatureDimension(int dim) {
  feature_dim_ = dim;
}

int Samples::FeatureDimension() const {
  return feature_dim_;
}

bool Samples::HaveSample(int phone, int state) const {
  if (phone >= samples_.size() || state >= samples_[phone].size())
    return false;
  return !samples_[phone][state].empty();
}

Sample* Samples::AddSample(int phone, int state) {
  CHECK_LT(phone, samples_.size());
  CHECK_GT(feature_dim_, 0);
  std::vector<SampleList> &p_sample = samples_[phone];
  if (state >= p_sample.size())
    p_sample.resize(state + 1);
  SampleList &sample_list = p_sample[state];
  sample_list.push_back(Sample(feature_dim_));
  return &sample_list.back();
}

void Statistics::AddObservation(const std::vector<float> &observation, float w) {
  CHECK_EQ(dimension(), observation.size());
  SetWeight(weight() + w);
  float *s = SumRef();
  float *s2 = Sum2Ref();
  for (int d = 0; d < dimension(); ++d, ++s, ++s2) {
    const float &o = observation[d];
    *s += o;
    *s2 += o * o;
  }
}

}  // namespace trainc
