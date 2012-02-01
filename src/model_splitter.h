// model_splitter.h
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
// model splitting optimizing acoustic likelihood and transducer size

#ifndef MODEL_SPLITTER_H_
#define MODEL_SPLITTER_H_

#include <list>
#include <set>
#include <vector>
#include "context_builder.h"
#include "phone_models.h"
#include "util.h"

using std::multiset;
using std::vector;
using std::list;

namespace trainc {

class StateCountingTransducer;
class AbstractSplitGenerator;
class SplitOptimizer;
class File;
class RecipeWriter;

// A hypothesized split of a state model.
// Includes the new AllophoneStateModels and the gain in likelihood
// achieved by the split.
struct SplitHypothesis {
  mutable AllophoneStateModel::SplitResult split;
  // TODO(rybach): split is mutable because we need access to the
  // non-const AllophoneStateModel* pointers in split, but because
  // SplitHypothesis is stored in a multiset, iterators grant only
  // const access to all members.
  // making split mutable is not dangerous though because only gain
  // is used as key for the multiset.
  const ContextQuestion *question;
  int position;
  float gain;
  ModelManager::StateModelRef model;
  // list<AllophoneStateModel*>::iterator model;
  SplitHypothesis(ModelManager::StateModelRef split_model,
                  AllophoneStateModel::SplitResult split_result,
                  const ContextQuestion *split_question,
                  int split_position,
                  float achieved_gain)
      : split(split_result), question(split_question),
        position(split_position), gain(achieved_gain),
        model(split_model) {}
  SplitHypothesis() {}
};

// comparison of two SplitHypothesis objects, based on gain.
struct SplitHypothesisGainCompare {
  bool operator()(const SplitHypothesis &a, const SplitHypothesis &b) const {
    return a.gain > b.gain;
  }
};


// splitting of tied HMM state models based on acoustic likelihood
// and transducer size.
// this class perform the actual optimization.
// methods are driven by ContextBuilder.
class ModelSplitter {
  typedef ContextBuilder::QuestionSet QuestionSet;
 public:
  typedef multiset<SplitHypothesis, SplitHypothesisGainCompare> SplitHypotheses;
  typedef SplitHypotheses::iterator SplitHypRef;

  ModelSplitter();
  virtual ~ModelSplitter();

  void InitModels(ModelManager *models) const;
  void InitSplitHypotheses(ModelManager *models);
  void SplitModels(ModelManager *models);

  void Cleanup();
  void SetSamples(const Samples *samples);
  void SetPhoneSymbols(const fst::SymbolTable *symbols);
  void SetPhoneInfo(const Phones *phone_info);
  void SetScorer(const Scorer *scorer);
  void SetContext(int num_left, int num_right, bool split_center);
  void SetMinGain(float min_gain);
  void SetMinContexts(int min_contexts);
  void SetMinObservations(int min_observations);
  void SetTargetNumModels(int num_models);
  void SetTargetNumStates(int num_states);
  void SetStatePenaltyWeight(float weight);
  void SetMaxHypotheses(int max_hyps);
  void SetIgnoreAbsentModels(bool ignore);
  void SetRecipeWriter(File *file);
  // set the transducer used for state counting.
  // the transducer will be modified throughout the optimization.
  // ownership stays at caller.
  void SetTransducer(StateCountingTransducer *t);
  vector<const QuestionSet*>* GetQuestions() {
    return &questions_;
  }
 protected:
  void CreateSplitHypotheses(const ModelManager::StateModelRef state_model,
                             bool ci_phone);
  virtual SplitHypRef FindBestSplit();

  void ApplySplit(ModelManager *models, SplitHypRef split_hyp);
  void RemoveModelHypothesis(SplitHypRef best_split);
  void DeleteSplit(AllophoneStateModel::SplitResult *split) const;

  const Samples *samples_;
  // split hypotheses are stored in a multiset ordered by achieved gain.
  SplitHypotheses split_hyps_;
  const fst::SymbolTable *phone_symbols_;
  const Phones *phone_info_;
  const Scorer *scorer_;
  float state_penaly_weight_;
  int num_left_contexts_;
  int target_num_models_, target_num_states_;
  int max_hyps_;
  bool split_center_, ignore_absent_models_;
  StateCountingTransducer *transducer_;
  vector<const QuestionSet*> questions_;
  AbstractSplitGenerator *generator_;
  SplitOptimizer *optimizer_;
  RecipeWriter *recipe_;
  DISALLOW_COPY_AND_ASSIGN(ModelSplitter);
};


}  // namespace trainc

#endif  // 
