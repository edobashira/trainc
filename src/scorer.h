// scorer.h
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
// Score computation for the split evaluation

#ifndef SCORER_H_
#define SCORER_H_

#include <cmath>
#include <list>
#include "sample.h"

namespace trainc {

class Scorer {
public:
  virtual ~Scorer() {}
  virtual float score(const Statistics &stats) const = 0;
};

// negative log-likelihood for Gaussian with diagonal covariance.
class MaximumLikelihoodScorer : public Scorer {
public:
  MaximumLikelihoodScorer(float variance_floor) :
    variance_floor_(variance_floor),
    pi_const_(std::log(M_PI + M_PI)) {}
  virtual ~MaximumLikelihoodScorer() {}

  virtual float score(const Statistics &stats) const {
    float n = stats.weight();
    float d = stats.dimension();
    double ll = 0.0;
    const float *sum = stats.sum();
    const float *sum2 = stats.sum2();
    for (int i = 0; i < d; ++i, ++sum, ++sum2) {
      float mean = *sum / n;
      float var = *sum2 / n;
      var -= mean * mean;
      if (var < variance_floor_) var = variance_floor_;
      ll += std::log(var);
    }
    return (.5 * n) * (d + d * pi_const_ + ll);
  }

protected:
  const float variance_floor_;
  const float pi_const_;
};

}  // namespace trainc

#endif  // 
