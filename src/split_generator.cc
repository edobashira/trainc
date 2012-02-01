// split_generator.cc
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

#include <ext/hash_set>
#include <limits>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "hash.h"
#include "split_generator.h"
#ifdef HAVE_THREADS
#include "thread.h"
#endif

namespace trainc {

using __gnu_cxx::hash_set;

bool AbstractSplitGenerator::IsValidSplit(
    const AllophoneStateModel::SplitResult &split) const {
  for (int c = 0; c < 2; ++c) {
    const AllophoneStateModel &state_model = *GetPairElement(split, c);
    if ((min_observations_ > 0 &&
        state_model.NumObservations() < min_observations_) ||
        (min_seen_contexts_ > 0 &&
        state_model.NumSeenContexts() < min_seen_contexts_)) {
      return false;
    }
  }
  return true;
}

void AbstractSplitGenerator::CreateSplitHypotheses(
    const ModelManager::StateModelRef state_model, bool center_only) {
  DCHECK(questions_);
  DCHECK(scorer_);
  DCHECK(hyps_);
  int from_context = (center_only ? 0 : -num_left_contexts_);
  int to_context = (center_only ? 0 : num_right_contexts_);
  hash_set<ContextSet, Hash<ContextSet>, Equal<ContextSet> > seen_contexts;
  for (int pos = from_context; pos <= to_context; ++pos) {
    if (!split_center_ && pos == 0)
      continue;
    const ContextSet &context = (*state_model)->GetContext().GetContext(pos);
    const QuestionSet& questions = *(*questions_)[pos + num_left_contexts_];
    seen_contexts.clear();
    seen_contexts.resize(questions.size());
    for (int q = 0; q < questions.size(); ++q) {
      const ContextQuestion *question = questions[q];
      ContextSet new_context = context;
      new_context.Intersect(question->GetPhoneSet(0));
      if (!new_context.IsEmpty() && !seen_contexts.count(new_context)) {
        // empty splits and redundant splits, i.e. questions yielding
        // an already used context, are ignored.
        seen_contexts.insert(new_context);
        AddHypothesis(state_model, pos, question);
      }
    }
  }
}

bool AbstractSplitGenerator::CreateSplit(SplitHypothesis *hyp) const {
  bool keep_hyp = true;
  hyp->gain = 0;
  AllophoneStateModel *model = *hyp->model;
  hyp->split = model->Split(hyp->position, *hyp->question);
  if (!(hyp->split.first && hyp->split.second)) {
    // model cannot be split into two models
    keep_hyp = false;
  } else {
    // fill the statistics of the two new models
    model->SplitData(hyp->position, &hyp->split);
    if (IsValidSplit(hyp->split)) {
      // only compute gain if the split is valid
      model->ComputeCosts(&hyp->split, *scorer_);
      hyp->gain = model->GetGain(hyp->split);
      if (hyp->gain < 0.0) {
        // negative gain shouldn't happen.
        LOG(WARNING) << "negative gain" << hyp->gain;
      }
      if (!IsEnoughGain(hyp->gain)) {
        keep_hyp = false;
      }
    } else {
      // too few observations
      keep_hyp = false;
    }
  }
  if (!keep_hyp) {
    // prune hypothesis
    delete hyp->split.first;
    delete hyp->split.second;
  }
  return keep_hyp;
}

AbstractSplitGenerator* AbstractSplitGenerator::Create(
    SplitHypotheses *target, int num_threads) {
  if (num_threads > 1) {
    VLOG(1) << "using parallel split generator. threads: " << num_threads;
    return new ParallelSplitGenerator(target, num_threads);
  } else {
    VLOG(1) << "using sequential split generator";
    return new SequentialSplitGenerator(target);
  }
}

// ===================================================================

void SequentialSplitGenerator::AddHypothesis(
    const ModelManager::StateModelRef state_model, int pos,
    const ContextQuestion *question) {
  SplitHypothesis hyp(state_model, AllophoneStateModel::SplitResult(NULL, NULL),
                      question, pos, -std::numeric_limits<float>::max());
  if (CreateSplit(&hyp)) {
    hyps_->insert(hyp);
  }
}

// ===================================================================

class SplitGeneratorTask : public SplitHypothesis {
public:
  SplitGeneratorTask(
      const ModelManager::StateModelRef state_model,
      int pos,
      const ContextQuestion *question)
      : SplitHypothesis(state_model,
                        AllophoneStateModel::SplitResult(NULL, NULL),
                        question, pos, -std::numeric_limits<float>::max()) {}
};

class SplitGeneratorMapper {
public:
  SplitGeneratorMapper(ParallelSplitGenerator *parent) : parent_(parent) {}
  SplitGeneratorMapper* Clone() const {
    return new SplitGeneratorMapper(parent_);
  }
  ~SplitGeneratorMapper() {
    STLDeleteElements(&hyps_);
  }
  void Map(SplitGeneratorTask *task) {
    if (parent_->CreateSplit(task))
      hyps_.push_back(task);
  }
  void Reset() {
    STLDeleteElements(&hyps_);
    hyps_.clear();
  }
  const std::vector<SplitHypothesis*>& GetHyps() const {
    return hyps_;
  }
protected:
  std::vector<SplitHypothesis*> hyps_;
  ParallelSplitGenerator *parent_;
};

class SplitGeneratorReducer {
public:

  typedef ModelSplitter::SplitHypotheses SplitHypotheses;
  SplitGeneratorReducer(SplitHypotheses *hyps) : hyps_(hyps) {}
  void Reduce(SplitGeneratorMapper *m) {
    typedef std::vector<SplitHypothesis*>::const_iterator Iter;
    for (Iter h = m->GetHyps().begin(); h != m->GetHyps().end(); ++h)
      hyps_->insert(**h);
  }
private:
  SplitHypotheses *hyps_;
};

#ifdef HAVE_THREADS
class ParallelSplitGenerator::Pool :
    public threads::ThreadPool<SplitGeneratorTask*, SplitGeneratorMapper,
                               SplitGeneratorReducer> {};
#else
// empty implementation, ParallelSplitGenerator cannot be used without
// a thread pool implementation.
class ParallelSplitGenerator::Pool {
public:
  Pool() {
    LOG(FATAL) << "cannot use ParallelSplitGenerator without threads";
  }
  void Init(int, SplitGeneratorMapper&) {}
  void Reset() {}
  void Submit(SplitGeneratorTask*) {}
  void Combine(SplitGeneratorReducer*) {}
};
#endif

ParallelSplitGenerator::ParallelSplitGenerator(
    SplitHypotheses *hyps, int num_threads) :
      AbstractSplitGenerator(hyps), pool_(new Pool()) {
  SplitGeneratorMapper mapper(this);
  pool_->Init(num_threads, mapper);
}

ParallelSplitGenerator::~ParallelSplitGenerator() {
  delete pool_;
}

void ParallelSplitGenerator::AddHypothesis(
    const ModelManager::StateModelRef state_model, int pos,
    const ContextQuestion *question) {
  pool_->Submit(new SplitGeneratorTask(state_model, pos, question));
}


void ParallelSplitGenerator::CreateSplitHypotheses(
    const ModelManager::StateModelRef state_model, bool center_only) {
  pool_->Reset();
  AbstractSplitGenerator::CreateSplitHypotheses(state_model, center_only);
  SplitGeneratorReducer reducer(hyps_);
  pool_->Combine(&reducer);
}

}  // namespace trainc {
