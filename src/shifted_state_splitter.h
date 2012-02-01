// shifted_state_splitter.h
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
// State splitting for a shifted LexiconTransducer

#ifndef SHIFTED_STATE_SPLITTER_H_
#define SHIFTED_STATE_SPLITTER_H_

#include "lexicon_state_splitter.h"

namespace trainc {

class ShiftedLexiconStateSplitter : public LexiconStateSplitter {
public:
  ShiftedLexiconStateSplitter(LexiconTransducer *l, int num_phones);
  virtual ~ShiftedLexiconStateSplitter();

  virtual void ApplySplit(int context_pos, const ContextQuestion *question,
                          AllophoneModel *old_model, int hmm_state,
                          const AllophoneModel::SplitResult &new_models);
  virtual void FinishSplit();

protected:
  typedef LexiconStateSplitter::ContextId ContextId;
  class Update;

  Update *update_;
};

}  // namespace trainc

#endif  // SHIFTED_STATE_SPLITTER_H_
