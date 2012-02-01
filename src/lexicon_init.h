// lexicon_init.h
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

#ifndef LEXICON_INIT_H_
#define LEXICON_INIT_H_

#include "lexicon_transducer.h"

namespace trainc {

// Converts a phoneme to word transducer into a LexiconTransducer.
// The transducer may not contain input epsilon cycles.
class LexiconTransducerInitializer {
public:
  LexiconTransducerInitializer(LexiconTransducer *target)
      : target_(target), num_phones_(target_->NumPhones()) {}
  virtual ~LexiconTransducerInitializer() {}
  void SetModels(const ModelManager &models);
  virtual void Build(const fst::StdExpandedFst &l);
protected:
  bool HasEpsilonCycle(const fst::StdExpandedFst &l) const;
  typedef LexiconTransducer::StateId StateId;
  typedef LexiconTransducer::Arc Arc;
  typedef LexiconTransducer::State State;
  LexiconTransducer *target_;
  std::vector<StateId> stateMap_;
  std::vector<const AllophoneModel*> phone_models_;
  const int num_phones_;
};

} // namespace trainc

#endif  // LEXICON_INIT_H_
