// lexicon_check.h
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
// Transducer consistency check.

#ifndef LEXICON_CHECK_H_
#define LEXICON_CHECK_H_

#include "lexicon_transducer.h"

namespace trainc {

// Checks the validity of a LexiconTransducer
// The following properties are tested:
//   - center phones of allophone model matches arc input label
//   - left context of allophone model matches input label of predecessor
//     arcs (including those reachable by epsilon arcs)
//   - left context of allophone model matches input label of successor
//     arcs (including those reachable by epsilon arcs)
//   - state context is subset of "full" state context, i.e. including
//     epsilon closure
class LexiconTransducerCheck {
  typedef LexiconTransducer::StateId StateId;
public:
  LexiconTransducerCheck(const Phones *phone_info)
      : phone_info_(phone_info),
        l_(NULL) {}
  void SetTransducer(const LexiconTransducer *l);

  bool IsValid() const;
private:
  void GetStateContext(StateId state_id, const LexiconState *state);
  bool VerifyArc(const PhoneContext &state_context,
                 const LexiconArc &arc) const;
  bool VerifyShiftedArc(const ContextSet &state_context,
                        const LexiconArc &arc) const;
  bool VerifyArcs(StateId state_id) const;
  bool VerifyShiftedArcs(StateId state_id) const;
  bool VerifyIncoming(const LexiconState *state,
                      const ContextSet &left_context) const;
  bool VerifyEmptyModel(StateId state) const;
  const Phones *phone_info_;
  const LexiconTransducer *l_;
  std::vector< std::pair<ContextSet, ContextSet> > state_context_;
};

}  // namespace trainc

#endif  // LEXICON_CHECK_H_
