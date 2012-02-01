// split_generator.h
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
// split hypotheses generatation

#ifndef SPLIT_GENERATOR_H_
#define SPLIT_GENERATOR_H_

#include <vector>
#include "model_splitter.h"
#include "phone_models.h"

namespace trainc {

class Scorer;

// Creates split hypotheses used in ModelSplitter.
// New hypotheses are added to hyps (set in in constructor)
class AbstractSplitGenerator {
  typedef ContextBuilder::QuestionSet QuestionSet;
public:
  typedef ModelSplitter::SplitHypotheses SplitHypotheses;

  explicit AbstractSplitGenerator(SplitHypotheses *hyps)
      : hyps_(hyps), num_left_contexts_(-1), num_right_contexts_(-1),
        split_center_(false), min_seen_contexts_(0), min_observations_(0),
        min_split_gain_(0), scorer_(NULL), questions_(NULL) {}
  virtual ~AbstractSplitGenerator() {}
  void SetMinObservations(int min_obs) {
    min_observations_ = min_obs;
  }
  void SetMinContexts(int min_contexts) {
    min_seen_contexts_ = min_contexts;
  }
  void SetMinGain(float min_gain) {
    min_split_gain_ = min_gain;
  }
  void SetScorer(const Scorer *scorer) {
    scorer_ = scorer;
  }
  void SetContext(int num_left_contexts, int num_right_contexts, bool center) {
    num_left_contexts_ = num_left_contexts;
    num_right_contexts_ = num_right_contexts;
    split_center_ = center;
  }
  void SetQuestions(const std::vector<const QuestionSet*> *questions) {
    questions_ = questions;
  }

  // Create split hypotheses for all possible splits of the given state model
  // using all context positions and all questions.
  // Only hypotheses meeting the requirements of min_observations_,
  // min_seen_contexts_ and min_split_gain_ are added.
  // If center_only == true, only splits for context position 0 are generated.
  virtual void CreateSplitHypotheses(
      const ModelManager::StateModelRef state_model, bool center_only);

  static AbstractSplitGenerator* Create(SplitHypotheses *target,
                                        int num_threads = 1);

protected:
  // Check if the split models have enough observations and seen contexts.
  bool IsValidSplit(const AllophoneStateModel::SplitResult &split) const;

  bool IsEnoughGain(float gain) const {
    return min_split_gain_ <= 0.0 || gain >= min_split_gain_;
  }

  virtual void AddHypothesis(const ModelManager::StateModelRef state_model,
                             int pos, const ContextQuestion *question) = 0;
  bool CreateSplit(SplitHypothesis *hyp) const;
  SplitHypotheses *hyps_;
  int num_left_contexts_, num_right_contexts_;
  bool split_center_;
  int min_seen_contexts_, min_observations_;
  float min_split_gain_;
  const Scorer *scorer_;
  const std::vector<const QuestionSet*> *questions_;
};


class SequentialSplitGenerator : public AbstractSplitGenerator {
public:
  SequentialSplitGenerator(SplitHypotheses *hyps) :
    AbstractSplitGenerator(hyps) {}
  virtual ~SequentialSplitGenerator() {}
protected:
  void AddHypothesis(const ModelManager::StateModelRef state_model, int pos,
                     const ContextQuestion *question);

};

class SplitGeneratorMapper;

class ParallelSplitGenerator : public AbstractSplitGenerator {
public:
  ParallelSplitGenerator(SplitHypotheses *hyps, int num_threads);
  virtual ~ParallelSplitGenerator();
  virtual void CreateSplitHypotheses(
      const ModelManager::StateModelRef state_model, bool center_only);

protected:
  class Pool;
  void AddHypothesis(const ModelManager::StateModelRef state_model, int pos,
                     const ContextQuestion *question);
  Pool *pool_;
  friend class SplitGeneratorMapper;
};

}  // namespace trainc {

#endif  // SPLIT_GENERATOR_H_
