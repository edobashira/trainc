// shifted_split_predictor.h
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
// Copyright 2012 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// State prediction for splits of a shifted LexiconTransducer

#ifndef SHIFTED_SPLIT_PREDICTOR_H_
#define SHIFTED_SPLIT_PREDICTOR_H_

#include "lexicon_split_predictor.h"

namespace trainc {

class EpsilonClosure;
class StateContexts;

class ShiftedLexiconSplitPredictor : public LexiconSplitPredictorBase {
  using LexiconSplitPredictorBase::State;
  using LexiconSplitPredictorBase::StateId;
  using LexiconSplitPredictorBase::Arc;
public:
  ShiftedLexiconSplitPredictor(const LexiconTransducer *l);
  virtual ~ShiftedLexiconSplitPredictor() {}

  virtual ShiftedLexiconSplitPredictor* Clone() const {
    ShiftedLexiconSplitPredictor *p = new ShiftedLexiconSplitPredictor(l_);
    p->SetDiscardAbsentModels(discard_absent_models_);
    return p;
  }

  virtual int Count(int context_pos, const ContextQuestion &question,
                    const AllophoneStateModel::AllophoneRefList &models,
                    int max_new_states);


  virtual bool NeedCount(int context_pos) const {
    return context_pos != 1;
  }

protected:
  int CountState(const ContextSet &state_context,
      const ContextQuestion &question) const;
  int CountPredecessors(StateId state, const ContextQuestion &question,
      const ContextSet *filter, ContextSet *context,
      StateSet *predecessors, bool *loop) const;
  int CountLeftSplit(const StateSet &all_states,
                     const ContextQuestion &question) const;
  int CountCenterSplit(const StateSet &all_states,
                       const ContextQuestion &question) const;

  EpsilonClosure *closure_;
  const StateContexts *contexts_;
private:
  DISALLOW_COPY_AND_ASSIGN(ShiftedLexiconSplitPredictor);
};

} // namespace trainc

#endif  // SHIFTED_SPLIT_PREDICTOR_H_
