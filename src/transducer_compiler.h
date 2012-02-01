// transducer_compiler.h
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
// Definition of the TransducerCompiler which constructs the
// final context depedency transducer.

#ifndef TRANSDUCER_COMPILER_H_
#define TRANSDUCER_COMPILER_H_

#include <ext/hash_map>
#include "fst/arc.h"
#include "fst/fst-decl.h"
#include "util.h"

using __gnu_cxx::hash_map;

namespace trainc {

class Phones;
class ConstructionalTransducer;
class HmmCompiler;
class Arc;
class State;
class AllophoneModel;

// Transforms a ConstructionalTransducer in an StdVectorFst.
// Before calling CreateTransducer(), data members have to be set.
class TransducerCompiler {
 public:
  TransducerCompiler();
  virtual ~TransducerCompiler();

  // Set the input transducer.
  void SetTransducer(const ConstructionalTransducer *t) {
    transducer_ = t;
  }

  // The the phone used at sequence boundaries.
  // Remark: Shifted phone index (offset -1), see ContextBuilder.
  void SetBoundaryPhone(int boundary_phone) {
    boundary_phone_ = boundary_phone;
  }

  // Create the final context dependency transducer.
  virtual fst::StdVectorFst* CreateTransducer();

  // The boundary state is the state that has the boundary_phone_ as
  // center phone and the boundary phone occurs in all but the last
  // context sets in the state's history, e.g. [ {},{sil,a} sil ].
  // This state is used to create the initial state of the C transducer.
  // See CreateStartState().
  static bool IsBoundaryState(const State &state, int boundary_phone);

 protected:
  virtual fst::StdArc::Label GetInputLabel(const Arc &arc) = 0;
  virtual fst::StdArc::Label GetOutputLabel(const Arc &arc) = 0;

 private:
  fst::StdArc::StateId GetState(fst::StdVectorFst *c, const State *state);
  void CreateStartState(const State &state, fst::StdVectorFst *c);
  void CreateArcs(const State &state, fst::StdArc::StateId,
                  bool eps_input, fst::StdVectorFst *c);
  bool IsBoundaryState(const State &state) const {
    return IsBoundaryState(state, boundary_phone_);
  }
  typedef hash_map<const State*, fst::StdArc::StateId, PointerHash<const State> > StateMap;
  const Phones *phone_info_;
  const ConstructionalTransducer *transducer_;
  StateMap state_map_;
  int boundary_phone_;
};


// Creates the context dependency transducer from a ConstructionalTransducer.
class HmmTransducerCompiler : public TransducerCompiler {
 public:
  HmmTransducerCompiler();
  virtual ~HmmTransducerCompiler() {}

  // Set the HmmCompiler used to construct the HMM and HMM state models.
  void SetHmmCompiler(const HmmCompiler *hmm_compiler) {
    hmm_compiler_ = hmm_compiler;
  }

  virtual fst::StdVectorFst* CreateTransducer();

 protected:
  virtual fst::StdArc::Label GetInputLabel(const Arc &arc);
  virtual fst::StdArc::Label GetOutputLabel(const Arc &arc);

 private:
  const HmmCompiler *hmm_compiler_;
};


// Transforms a ConstructionalTransducer to an StdVectorFst,
// keeping the model to label mapping.
class ModelTransducerCompiler : public TransducerCompiler {
public:
  ModelTransducerCompiler() {}
  virtual ~ModelTransducerCompiler() {}

  void GetLabelMap(std::vector<const AllophoneModel*> *map) const;

 protected:
  virtual fst::StdArc::Label GetInputLabel(const Arc &arc);
  virtual fst::StdArc::Label GetOutputLabel(const Arc &arc);

 private:
  typedef hash_map<const AllophoneModel*, fst::StdArc::Label,
                   PointerHash<const AllophoneModel> > LabelMap;
  LabelMap label_map_;
};
}  // namespace trainc

#endif  // TRANSDUCER_COMPILER_H_
