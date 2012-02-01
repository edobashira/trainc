// lexicon_split_predictor.h
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
// Copyright 2011 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// State prediction for splits of an un-shifted LexiconTransducer

#ifndef LEXICON_SPLIT_PREDICTOR_H_
#define LEXICON_SPLIT_PREDICTOR_H_

#include <ext/hash_set>
#include "hash.h"
#include "lexicon_transducer.h"
#include "lexicon_state_splitter.h"
#include "split_predictor.h"

using __gnu_cxx::hash_set;

namespace trainc {

// Counts the number of new states required to apply a given model
// split in a LexiconTransducer.
class LexiconSplitPredictorBase : public AbstractSplitPredictor {
public:
  LexiconSplitPredictorBase(const LexiconTransducer *l);
  virtual ~LexiconSplitPredictorBase() {}
  virtual bool IsThreadSafe() const {
    // Count() cannot be called in parallel because it modifies
    // the (shared) LexiconStateSiblings, EpsilonClosure, and
    // StateContexts objects of the LexiconTransducer l_
    return false;
  }
  virtual void SetDiscardAbsentModels(bool discard) {
    discard_absent_models_ = discard;
  }

protected:
  typedef LexiconTransducer::State State;
  typedef LexiconTransducer::StateId StateId;
  typedef LexiconTransducer::Arc Arc;
  typedef hash_set<StateId> StateSet;

  void GetStates(int context_pos,
                 const AllophoneStateModel::AllophoneRefList &models,
                 const ContextQuestion &question, bool source_state,
                 vector<StateId> *states) const;

  bool ModelExists(const AllophoneStateModel::AllophoneRefList &models) const;

  const LexiconTransducer *l_;
  int num_phones_;
  bool discard_absent_models_;
};


class LexiconSplitPredictor : public LexiconSplitPredictorBase {
  using LexiconSplitPredictorBase::State;
  using LexiconSplitPredictorBase::StateId;
  using LexiconSplitPredictorBase::Arc;
public:
  LexiconSplitPredictor(const LexiconTransducer *l);
  virtual ~LexiconSplitPredictor() {}

  virtual LexiconSplitPredictor* Clone() const {
    LexiconSplitPredictor *p = new LexiconSplitPredictor(l_);
    p->SetDiscardAbsentModels(discard_absent_models_);
    return p;
  }

  virtual int Count(int context_pos, const ContextQuestion &question,
                    const AllophoneStateModel::AllophoneRefList &models,
                    int max_new_states);


  virtual bool NeedCount(int context_pos) const {
    return context_pos != 0;
  }

  virtual void Init();

protected:
  typedef hash_map<StateId, ContextSet> StateToContextMap;
  typedef hash_set<const AllophoneModel*, PointerHash<AllophoneModel> > ModelSet;
  typedef hash_set<StateId> StateSet;
  typedef LexiconStateSplitter::ContextId ContextId;
  typedef pair<ContextSet, ContextSet> ContextPair;
  class NewState {
  public:
    StateId origin;
    ContextPair context;
    NewState(StateId o, const ContextPair &c) :
      origin(o), context(c) {}
    size_t HashValue() const {
      size_t h = origin;
      HashCombine(h, context.first.HashValue());
      HashCombine(h, context.second.HashValue());
      return h;
    }
    bool IsEqual(const NewState &o) const {
      return origin == o.origin && context.first.IsEqual(o.context.first)
          && context.second.IsEqual(o.context.second);
    }
  };
  typedef hash_set<NewState, Hash<NewState>, Equal<NewState> > NewStateSet;

  bool HasOtherModels(StateId state_id, const ModelSet &models);
  bool KeepState(StateId state_id, const ModelSet &models);
  int CountState(const StateSet &all_states, StateId state_id,
                 const ContextSet &state_context,
                 const ContextQuestion &question, bool split_right,
                 const ModelSet &models);
  void AddStates(const StateSet &all_states, StateId state_id,
                 ContextId context_id, const ContextQuestion &question);
  const LexiconStateSiblings *siblings_;

  NewStateSet new_states_;
  std::vector<bool> has_other_model_, know_state_;
  bool deterministic_;

private:
  DISALLOW_COPY_AND_ASSIGN(LexiconSplitPredictor);
};

}  // namespace trainc

#endif  // LEXICON_SPLIT_PREDICTOR_H_
