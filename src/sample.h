// sample.h
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
// Training samples

#ifndef CONTEXT_SAMPLE_H_
#define CONTEXT_SAMPLE_H_

#include <algorithm>
#include <functional>
#include <list>
#include <vector>
#include "util.h"
#include "debug.h"

namespace trainc {

// Sufficient statistics for a Gaussian distribution.
// Sum of observations, sum of squared observations,
// number of (weighted) observations.
class Statistics {
public:
  Statistics() : dim_(-1) {}

  Statistics(int dimension) : dim_(dimension), data_(dim_ * 2 + 1, 0.0) {}

  void Reset(int dimension) {
    dim_ = dimension;
    data_.resize(dim_ * 2 + 1);
    std::fill(data_.begin(), data_.end(), 0.0);
  }

  // dimensionality of the features
  int dimension() const { return dim_; }
  // number of (weighted) observations
  float weight() const { return data_[0]; }
  // set the weight
  void SetWeight(float w) { data_[0] = w; }
  // sum of observations
  const float* sum() const { return &data_[1]; }
  // mutable access to sum
  float* SumRef() { return &data_[1]; }
  // sum of squared observations
  const float* sum2() const { return sum() + dim_; }
  // mutable access to squared sum
  float* Sum2Ref() { return SumRef() + dim_; }

  // accumulate statistics
  void Accumulate(const Statistics &other) {
    DCHECK_EQ(dimension(), other.dimension());
    std::transform(data_.begin(), data_.end(),
                   other.data_.begin(), data_.begin(), std::plus<float>());
  }

  void AddObservation(const std::vector<float> &observation, float weight = 1.0);
private:
  int dim_;
  std::vector<float> data_;
};

// A training sample consisting of left and right context
// and pointer to the Statistics
struct Sample {
  Statistics stat;
  std::vector<int> left_context_;
  std::vector<int> right_context_;
  Sample(int feature_dim) : stat(feature_dim) {}
};



// Collection of all Sample and Statistics objects.
// SetNumPhones and SetFeatureDimension has be to be called
// before the first call to AddSample.
class Samples {
public:
  typedef std::list<Sample> SampleList;

  Samples();

  // set the number of occuring phones
  void SetNumPhones(int num_phones);
  // number of occuring phones
  int NumPhones() const;
  // set the dimensionality of the feature vectors
  void SetFeatureDimension(int dim);
  // dimensionality of the feature vectors
  int FeatureDimension() const;

  // Create a new Sample object for the given phone, state
  // and return a pointer to it.
  // The returned pointer is only guaranteed to be valid
  // until AddSample() is called again.
  Sample* AddSample(int phone, int state);

  bool HaveSample(int phone, int state) const;

  // Return the list of samples for the given phone and HMM state.
  const SampleList& GetSamples(int phone, int state) const {
    CHECK_LT(phone, samples_.size());
    CHECK_LT(state, samples_[phone].size());
    return samples_[phone][state];
  }

  // Return the maximum state number for the given phone
  int NumStates(int phone) const {
    CHECK_LT(phone, samples_.size());
    return samples_[phone].size();
  }

private:
  int feature_dim_;
  std::vector< std::vector<SampleList> > samples_;

  DISALLOW_COPY_AND_ASSIGN(Samples);
};

}  // namespace trainc

#endif /* CONTEXT_SAMPLE_H_ */
