// model_splitter.cc
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

#include "fst/symbol-table.h"
#include "model_splitter.h"
#include "recipe.h"
#include "scorer.h"
#include "split_generator.h"
#include "split_optimizer.h"
#include "split_predictor.h"
#include "transducer.h"



DECLARE_int32(num_threads);

namespace trainc {


ModelSplitter::ModelSplitter()
    : samples_(NULL),
      phone_symbols_(NULL),
      phone_info_(NULL),
      scorer_(NULL),
      state_penaly_weight_(0),
      num_left_contexts_(0),
      target_num_models_(0),
      target_num_states_(0),
      max_hyps_(0),
      split_center_(false),
      ignore_absent_models_(false),
      transducer_(NULL),
      generator_(AbstractSplitGenerator::Create(&split_hyps_,
                                                FLAGS_num_threads)),
      optimizer_(NULL),
      recipe_(NULL) {
  generator_->SetQuestions(&questions_);
}

ModelSplitter::~ModelSplitter() {
  delete samples_;
  delete generator_;
  delete scorer_;
  delete optimizer_;
  delete recipe_;
}

void ModelSplitter::SetSamples(const Samples *samples) {
  samples_ = samples;
}

void ModelSplitter::SetPhoneSymbols(const fst::SymbolTable *symbols) {
  phone_symbols_ = symbols;
}

void ModelSplitter::SetPhoneInfo(const Phones *phone_info) {
  phone_info_ = phone_info;
}

void ModelSplitter::SetScorer(const Scorer *scorer) {
  generator_->SetScorer(scorer);
}

void ModelSplitter::SetContext(int num_left, int num_right, bool split_center) {
  num_left_contexts_ = num_left;
  generator_->SetContext(num_left, num_right, split_center);
}

void ModelSplitter::SetMinGain(float min_gain) {
  generator_->SetMinGain(min_gain);
}

void ModelSplitter::SetMinContexts(int min_contexts) {
  generator_->SetMinContexts(min_contexts);
}

void ModelSplitter::SetMinObservations(int min_observations) {
  generator_->SetMinObservations(min_observations);
}

void ModelSplitter::SetTargetNumModels(int num_models) {
  target_num_models_ = num_models;
}

void ModelSplitter::SetTargetNumStates(int num_states) {
  target_num_states_ = num_states;
}

void ModelSplitter::SetStatePenaltyWeight(float weight) {
  state_penaly_weight_ = weight;
  VLOG(1) << "using state penalty weight " << state_penaly_weight_;
  if (optimizer_)
    optimizer_->SetWeight(state_penaly_weight_);
}

void ModelSplitter::SetMaxHypotheses(int max_hyps) {
  max_hyps_ = max_hyps;
  if (optimizer_)
    optimizer_->SetMaxHyps(max_hyps);
}

void ModelSplitter::SetIgnoreAbsentModels(bool ignore) {
  ignore_absent_models_ = ignore;
  if (optimizer_)
    optimizer_->SetIgnoreAbsentModels(ignore);
}

void ModelSplitter::SetRecipeWriter(File *file) {
  recipe_ = new RecipeWriter(file);
}

void ModelSplitter::SetTransducer(StateCountingTransducer *t) {
  transducer_ = t;
  optimizer_ = SplitOptimizer::Create(split_hyps_, *transducer_,
                                      FLAGS_num_threads);
  optimizer_->SetMaxHyps(max_hyps_);
  optimizer_->SetWeight(state_penaly_weight_);
  optimizer_->SetIgnoreAbsentModels(ignore_absent_models_);
}

// Initialize the statistics of the initial HMM state models.
void ModelSplitter::InitModels(ModelManager *models) const {
  CHECK_NOTNULL(samples_);
  CHECK(!questions_.empty());
  CHECK_NOTNULL(generator_);
  CHECK_NOTNULL(optimizer_);
  for (ModelManager::StateModelRef m = models->GetStateModelsRef()->begin();
      m != models->GetStateModelsRef()->end(); ++m) {
    AllophoneStateModel *state_model = *m;
    CHECK_EQ(state_model->GetAllophones().size(), 1);
    const vector<int> &phones = state_model->GetAllophones().front()->phones();
    int state = state_model->state();
    bool have_data = false;
    for (vector<int>::const_iterator p = phones.begin();
        p != phones.end(); ++p) {
      HmmStateStat *state_stat = new HmmStateStat(*p);
      if (samples_->HaveSample(*p + 1, state)) {
        const Samples::SampleList &sample_list =
            samples_->GetSamples(*p + 1, state);
        state_stat->SetStats(sample_list);
        state_model->AddStatistics(state_stat);
        VLOG(2) << "statistics for phone=" << phone_symbols_->Find(*p + 1)
                << " state=" << state
                << ": " << state_stat->NumObservations();
        have_data = true;
      } else {
        REP(WARNING) << "no statistics for "
                     << phone_symbols_->Find(*p + 1)
                     << " state " << state;
        delete state_stat;
      }
    }
    if (have_data) {
      VLOG(2) << "statistics for state model: "
              << state_model->NumObservations();
    } else {
      REP(FATAL) << "no statistics for unit "
                 << phone_symbols_->Find(phones.front() + 1)
                 << " state " << state;
    }
  }
}

// Create all split hypotheses for all existing state models.
void ModelSplitter::InitSplitHypotheses(ModelManager *models) {
  split_hyps_.clear();
  for (ModelManager::StateModelRef sm = models->GetStateModelsRef()->begin();
      sm != models->GetStateModelsRef()->end(); ++sm) {
    const AllophoneStateModel &state_model = *(*sm);
    CHECK_GT(state_model.GetAllophones().size(), 0);
    const vector<int> &phones = state_model.GetAllophones().front()->phones();
    int phone = phones.front();
    bool ci_phone = phone_info_->IsCiPhone(phone);
    if (!ci_phone || phones.size() > 1) {
      CreateSplitHypotheses(sm, ci_phone);
    }
  }
  VLOG(1) << "initial split hypotheses: " << split_hyps_.size();
}



// Create split hypotheses for all possible splits of the given state model
// using all context positions and all questions.
// If ci_phone == true, only center phone splits are generated.
void ModelSplitter::CreateSplitHypotheses(
    const ModelManager::StateModelRef state_model, bool ci_phone) {
  generator_->CreateSplitHypotheses(state_model, ci_phone);
}

// Find the ModelSplitHypothesis in splits_hyps_ with best the best score.
// The score is computed using
//   G : the gain achieved by the split
//   S : the number of new states required to distinguish between the two
//       new state models
//   w : the weight of the S, i.e. "state penalty" (state_penalty_weight_)
//   score = G - w * S
ModelSplitter::SplitHypRef ModelSplitter::FindBestSplit() {
  DCHECK(optimizer_);
  int best_new_states = -1, best_rank = -1, num_counts = 0;
  float best_score = 0;
  SplitHypRef best_hyp = optimizer_->FindBestSplit(
      &num_counts, &best_score, &best_new_states, &best_rank);
  if (best_hyp == split_hyps_.end())
    return best_hyp;
  int best_phone =
      (*best_hyp->model)->GetAllophones().front()->phones().front();

  VLOG(1)
      << "num_hyps: " << split_hyps_.size()
      << " num_counts: " << num_counts
      << " best: score=" << best_score
      << " gain=" << best_hyp->gain
      << " new states=" << best_new_states
      << " position=" << best_hyp->position
      << " question=" << best_hyp->question->Name()
      << " phone=" << best_phone << "=" << phone_symbols_->Find(best_phone + 1)
      << " state=" << (*best_hyp->model)->state()
      << " rank=" << best_rank;
  return best_hyp;
}

// Apply the split hypothesis (model_hyp and split_hyp) to the
// transducer, store the models in the ModelMananger, and create
// ModelSplitHypotheses for the split state models.
void ModelSplitter::ApplySplit(ModelManager *models, SplitHypRef split_hyp) {
  const int hmm_state = (*split_hyp->model)->state();
  const int position = split_hyp->position;
  const int phone =
      (*split_hyp->model)->GetAllophones().front()->phones().front();
  const bool ci_phone = phone_info_->IsCiPhone(phone);

  // store new models in the ModelManager, delete old models.
  ModelSplit split_result;
  models->ApplySplit(split_hyp->position, split_hyp->model,
                     &split_hyp->split, &split_result);

  // create states and arcs in the context dependency transducer
  typedef vector<AllophoneModelSplit>::iterator ModelIter;
  for (ModelIter m = split_result.phone_models.begin();
      m != split_result.phone_models.end(); ++m) {
    transducer_->ApplyModelSplit(position, split_hyp->question, m->old_model,
        hmm_state, m->new_models);
  }
  transducer_->FinishSplit();

  models->DeleteOldModels(&split_result.phone_models);

  // create new ModelSplitHypotheses for the new state models
  for (int c = 0; c < 2; ++c) {
    ModelManager::StateModelRef new_state_model =
        GetPairElement(split_result.state_models, c);
    CreateSplitHypotheses(new_state_model, ci_phone);
  }
}

// Remove the all SplitHypothesis from split_hyps_ which have the same model
// as best_split and delete all of their
// AllophoneStateModels, except for the AllophoneStateModels in best_split,
// because they have been committed to the ModelManager.
void ModelSplitter::RemoveModelHypothesis(SplitHypRef best_split) {
  // iterator will get invalid if we apply erase, therefore store the iterators
  // and apply erase afterwards.
  // TODO(rybach): add a model to hyp map?
  vector<SplitHypRef> to_remove;
  for (SplitHypRef s = split_hyps_.begin(); s != split_hyps_.end(); ++s) {
    if (s->model == best_split->model) {
      if (s != best_split)
        DeleteSplit(&s->split);
      to_remove.push_back(s);
    }
  }
  for (vector<SplitHypRef>::iterator i = to_remove.begin(); i != to_remove.end(); ++i)
    split_hyps_.erase(*i);
}

// Delete the AllophoneStateModels that have been created by a split
// but have not been committed (i.e. stored in the ModelManager).
void ModelSplitter::DeleteSplit(
    AllophoneStateModel::SplitResult *split) const {
  delete split->first;
  split->first = NULL;
  delete split->second;
  split->second = NULL;
}

// Delete all remaining AllophoneStateModels and AllophoneModels in split_hyps_.
// These models have been created by a split but they are not owned by the
// ModelManager because the split hasn't been applied.
void ModelSplitter::Cleanup() {
  for (SplitHypRef hyp = split_hyps_.begin(); hyp != split_hyps_.end(); ++hyp)
    DeleteSplit(&hyp->split);
  split_hyps_.clear();
}

// Iteratively split the state models by selecting in each iteration the
// split with the highest score (see FindBestSplit).
// The splitting ends, when no state model can be split or the number of
// target models is reached.
void ModelSplitter::SplitModels(ModelManager *models) {
  int num_models = models->NumStateModels();
  int num_states = transducer_->NumStates();
  int num_new_states = 0;
  if (recipe_) {
    recipe_->SetQuestions(num_left_contexts_, &questions_);
    recipe_->Init();
  }
  while (!split_hyps_.empty() &&
         (target_num_models_ == 0 || num_models < target_num_models_) &&
         (target_num_states_ == 0 || num_states < target_num_states_)) {
    SplitHypRef best_split = FindBestSplit();
    if (best_split == split_hyps_.end()) {
      REP(INFO) << "no valid split found";
      break;
    }
    if (recipe_) recipe_->AddSplit(*best_split);
    ApplySplit(models, best_split);
    RemoveModelHypothesis(best_split);
    num_models = models->NumStateModels();
    num_new_states = -num_states;
    num_states = transducer_->NumStates();
    num_new_states += num_states;
    REP(INFO) << "#models: " << num_models << " "
              << "#states: " << num_states << " "
              << "new states: " << num_new_states;
  }
}


}  // namespace trainc {
