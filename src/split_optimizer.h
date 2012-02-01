// split_optimizer.h
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
// \file
// optimization of split hypotheses w.r.t required transducer states

#ifndef SPLIT_OPTIMIZER_H_
#define SPLIT_OPTIMIZER_H_

#include "model_splitter.h"

namespace trainc {

class StateCountingTransducer;
class AbstractSplitPredictor;

// Split optimization, i.e. re-ranking of split hypotheses, using
// the transducer state count.
class SplitOptimizer {
protected:
  typedef ModelSplitter::SplitHypotheses SplitHypotheses;
  typedef ModelSplitter::SplitHypRef SplitHypRef;
public:
  SplitOptimizer(const SplitHypotheses &hyps, const StateCountingTransducer &t)
      : split_hyps_(hyps), t_(t), weight_(0), max_hyps_(0),
        ignore_absent_model_(false) {}
  virtual ~SplitOptimizer() {}
  void SetWeight(float weight) {
    weight_ = weight;
    VLOG(1) << "SplitOptimizer::SetWeight " << weight_;
  }
  void SetMaxHyps(int max_hyps) {
    max_hyps_ = max_hyps;
  }
  void SetIgnoreAbsentModels(bool ignore) {
    ignore_absent_model_ = ignore;
  }

  virtual SplitHypRef FindBestSplit(int *num_counts_, float *best_score,
                                    int *new_states, int *rank) = 0;

  static SplitOptimizer* Create(const SplitHypotheses &hyps,
                                const StateCountingTransducer &t,
                                int num_threads = 1);
protected:
  const SplitHypotheses &split_hyps_;
  const StateCountingTransducer &t_;
  float weight_;
  int max_hyps_;
  bool ignore_absent_model_;
};

// Sequential optimization.
// This is the default implementation, used when either multi-threading
// is not available or num_threads <= 1
class SequentialSplitOptimizer : public SplitOptimizer {
public:
  SequentialSplitOptimizer(const SplitHypotheses &hyps,
                           const StateCountingTransducer &t);
  virtual ~SequentialSplitOptimizer();
  SplitHypRef FindBestSplit(int *num_counts_, float *best_score,
                            int *new_states, int *rank);
protected:
  AbstractSplitPredictor *predictor_;
};


// Optimization using shared memory parallelization.
// If multi-threading is not available, this class is not functional.
class ParallelSplitOptimizer : public SplitOptimizer {
public:
  ParallelSplitOptimizer(const SplitHypotheses &hyps,
                         const StateCountingTransducer &t,
                         int num_threads);
  virtual ~ParallelSplitOptimizer();
  SplitHypRef FindBestSplit(int *num_counts_, float *best_score,
                            int *new_states, int *rank);
protected:
  void Init();
  class Pool;
  Pool *pool_;
  AbstractSplitPredictor *predictor_;
  int num_threads_;
  bool need_init_;
  friend class SplitOptimizerMapper;
};


}  // namespace trainc

#endif  // SPLIT_OPTIMIZER_H_
