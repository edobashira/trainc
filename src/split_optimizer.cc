// split_optimizer.cc
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_THREADS
#include "thread.h"
#endif
#include "split_optimizer.h"
#include "split_predictor.h"
#include "transducer.h"

namespace trainc {

SplitOptimizer* SplitOptimizer::Create(
    const SplitHypotheses &hyps, const StateCountingTransducer &t,
    int num_threads) {
  if (num_threads > 1) {
    VLOG(1) << "using parallel split optimizer. threads: " << num_threads;
    return new ParallelSplitOptimizer(hyps, t, num_threads);
  } else {
    VLOG(1) << "using sequential split optimizer.";
    return new SequentialSplitOptimizer(hyps, t);
  }
}

SequentialSplitOptimizer::SequentialSplitOptimizer(
    const SplitHypotheses &hyps, const StateCountingTransducer &t)
    : SplitOptimizer(hyps, t) {
  predictor_ = t_.CreateSplitPredictor();
}

SequentialSplitOptimizer::~SequentialSplitOptimizer() {
  delete predictor_;
}
SplitOptimizer::SplitHypRef SequentialSplitOptimizer::FindBestSplit(
    int *num_counts, float *best_score, int *new_states, int *rank) {
  if (weight_ == .0) {
    *num_counts = 0;
    *best_score = split_hyps_.begin()->gain;
    *new_states = 0;
    *rank = 0;
    return split_hyps_.begin();
  }
  predictor_->SetDiscardAbsentModels(ignore_absent_model_);
  predictor_->Init();
  float best = -std::numeric_limits<float>::max();
  SplitHypRef best_hyp = split_hyps_.end();
  typedef vector<SplitHypothesis>::const_iterator SplitIter;
  int best_new_states = -1, best_rank = -1;
  int h = 0, c = 0;
  size_t max_hyp = max_hyps_ ? std::min(size_t(max_hyps_), split_hyps_.size())
                             : split_hyps_.size();
  for (SplitHypRef hyp = split_hyps_.begin(); h < max_hyp; ++hyp, ++h) {
    const SplitHypothesis &split = *hyp;
    if (split.gain >= best) {
      // compute number of states only if a score higher than the
      // current best_score is possible.
      int num_new_states = 0;
      if (predictor_->NeedCount(split.position)) {
        const AllophoneStateModel::AllophoneRefList &allophones =
            (*split.model)->GetAllophones();
        int max_states = 0;
        if (h) {
          // the computation of new states can be stopped if the resulting
          // score will be lower than the current best score
          max_states = std::ceil((split.gain - best) / weight_) + 1;
        }
        num_new_states = predictor_->Count(
            split.position, *split.question, allophones, max_states);
        ++c;
      }
      if (num_new_states != AbstractSplitPredictor::kInvalidCount) {
        float score = split.gain - weight_ * num_new_states;
        if (score > best) {
          best = score;
          best_hyp = hyp;
          best_new_states = num_new_states;
          best_rank = h;
        }
      }
    } else {
      // hypotheses are sorted, score can only get lower
      break;
    }
  }
  *num_counts = c;
  *best_score = best;
  *new_states = best_new_states;
  *rank = best_rank;
  return best_hyp;
}

class SplitOptimizerTask {
  typedef ModelSplitter::SplitHypRef SplitHypRef;
public:
  SplitOptimizerTask() : rank_(-1) {}
  SplitOptimizerTask(const SplitHypRef &hyp, int rank)
      : hyp_(hyp), rank_(rank) {}
  SplitHypRef hyp_;
  int rank_;
};

class SplitOptimizerMapper {
  typedef ModelSplitter::SplitHypRef SplitHypRef;
public:
  SplitOptimizerMapper(AbstractSplitPredictor *predictor,
                       float weight)
      : new_states(-1), rank(-1), counts(0),
        best_score(-std::numeric_limits<float>::max()),
        weight_(weight), predictor_(predictor) {
  }
  SplitOptimizerMapper* Clone() const {
    return new SplitOptimizerMapper(predictor_->Clone(), weight_);
  }
  ~SplitOptimizerMapper() {
    delete predictor_;
  }
  void Reset() {
    new_states = rank = -1;
    counts = 0;
    best_score = -std::numeric_limits<float>::max();
    predictor_->Init();
  }
  void Map(const SplitOptimizerTask &task) {
    const SplitHypRef &hyp = task.hyp_;
    int max_states = 0;
    if (counts)
      max_states = std::ceil((hyp->gain - best_score) / weight_) + 1;
    int ns = predictor_->Count(hyp->position, *hyp->question,
                               (*hyp->model)->GetAllophones(), max_states);
    ++counts;
    if (ns != AbstractSplitPredictor::kInvalidCount) {
      float score = hyp->gain - weight_ * ns;
      if (score > best_score) {
        best_score = score;
        best_hyp = hyp;
        new_states = ns;
        rank = task.rank_;
      }
    }
  }
  SplitHypRef best_hyp;
  int new_states, rank, counts;
  float best_score;
protected:
  float weight_;
  AbstractSplitPredictor *predictor_;
private:
  DISALLOW_COPY_AND_ASSIGN(SplitOptimizerMapper);
};

class SplitOptimizerReducer {
  typedef ModelSplitter::SplitHypRef SplitHypRef;
public:
  SplitOptimizerReducer(SplitHypRef best_hyp, float *score,
                        int *states, int *rank, int *counts)
      : new_states_(states), best_rank_(rank), counts_(counts),
        best_score_(score), best_hyp_(best_hyp) {}

  void Reduce(SplitOptimizerMapper *m) {
    if (m->best_score > *best_score_ ||
        (m->best_score == *best_score_ && m->rank < *best_rank_)) {
      *best_score_ = m->best_score;
      *new_states_ = m->new_states;
      *best_rank_ = m->rank;
      best_hyp_ = m->best_hyp;
    }
    *counts_ += m->counts;
  }
  const SplitHypRef& BestHyp() const {
    return best_hyp_;
  }
protected:
  int *new_states_, *best_rank_, *counts_;
  float *best_score_;
  SplitHypRef best_hyp_;
};

#ifdef HAVE_THREADS
class ParallelSplitOptimizer::Pool :
   public threads::ThreadPool<SplitOptimizerTask, SplitOptimizerMapper,
                              SplitOptimizerReducer> {};
#else
class ParallelSplitOptimizer::Pool {
public:
  Pool() {
    LOG(FATAL) << "cannot use ParallelSplitOptimizer without threads";
  }
  void Init(int, const SplitOptimizerMapper&) {}
  void Submit(const SplitOptimizerTask&) {}
  void Combine(SplitOptimizerReducer*) {}
  void Reset() {}
};
#endif

ParallelSplitOptimizer::ParallelSplitOptimizer(
    const SplitHypotheses &hyps, const StateCountingTransducer &t,
    int num_threads)
    : SplitOptimizer(hyps, t), pool_(new Pool()),
      predictor_(t.CreateSplitPredictor()),
      num_threads_(num_threads), need_init_(true) {
  CHECK(predictor_->IsThreadSafe());
}

// separate initialization require because weight_ is not known
// in the constructor.
void ParallelSplitOptimizer::Init() {
  if (max_hyps_)
    LOG(WARNING) << "cannot use max_hyps in ParallelSplitOptimizer";
  predictor_->SetDiscardAbsentModels(ignore_absent_model_);
  SplitOptimizerMapper mapper(predictor_, weight_);
  AbstractSplitPredictor *own_predictor = predictor_->Clone();
  pool_->Init(num_threads_, mapper);
  // predictor_ is deleted in SplitOptimizerMapper
  predictor_ = own_predictor;
  need_init_ = false;
}

ParallelSplitOptimizer::~ParallelSplitOptimizer() {
  delete pool_;
  delete predictor_;
}

SplitOptimizer::SplitHypRef ParallelSplitOptimizer::FindBestSplit(
    int *num_counts, float *best_score, int *new_states, int *rank) {
  if (need_init_) Init();
  pool_->Reset();
  float best = -std::numeric_limits<float>::max();
  int best_rank = -1, best_new_states = -1, r = 0, t = 0;
  SplitHypRef best_hyp = split_hyps_.end();
  for (SplitHypRef hyp = split_hyps_.begin();
      hyp != split_hyps_.end(); ++hyp, ++r) {
    if (hyp->gain > best) {
      ++t;
      if (predictor_->NeedCount(hyp->position)) {
        pool_->Submit(SplitOptimizerTask(hyp, r));
      } else {
        best = hyp->gain;
        best_hyp = hyp;
        best_rank = r;
        best_new_states = 0;
      }
    } else {
      break;
    }
  }
  *num_counts = 0;
  SplitOptimizerReducer reducer(best_hyp, &best, &best_new_states, &best_rank,
                                num_counts);
  pool_->Combine(&reducer);
  VLOG(1) << "# splits evaluated: " << t;
  best_hyp = reducer.BestHyp();
  *best_score = best;
  *new_states = best_new_states;
  *rank = best_rank;
  return best_hyp;
}

}  // namespace trainc
