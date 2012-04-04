// composed_transducer.cc
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

#include <vector>
#include <map>
#include <deque>
#include "fst/arcsort.h"
#include "fst/compose.h"
#include "fst/map.h"
#include "fst/project.h"
#include "fst/queue.h"
#include "fst/rmepsilon.h"
#include "fst/state-table.h"
#include "fst/visit.h"
#include "composed_transducer.h"
#include "fst_interface.h"
#include "split_predictor.h"
#include "transducer_compiler.h"

namespace trainc {

// collects predecessor states
template<class A>
class PredecessorVisitor {
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef ComposedTransducer::PredecessorList PredecessorList;
public:
  PredecessorVisitor(std::vector<PredecessorList> *p, Label num_labels,
                     Label label_offset)
      : predecessors_(p), empty_set_(num_labels), offset_(label_offset) {}

  void InitVisit(const fst::Fst<A> &fst) {
    predecessors_->clear();
  }
  bool InitState(StateId s, StateId root) {
    if (s >= predecessors_->size())
      predecessors_->resize(s + 1);
    return true;
  }
  bool WhiteArc(StateId s, const Arc &a) {
    SetPredecessor(s, a.nextstate, a.olabel);
    return true;
  }
  bool GreyArc(StateId s, const Arc &a) {
    SetPredecessor(s, a.nextstate, a.olabel);
    return true;
  }
  bool BlackArc(StateId s, const Arc &a) {
    SetPredecessor(s, a.nextstate, a.olabel);
    return true;
  }
  void FinishState(StateId s) {}
  void FinishVisit() {}

  int NumStates() const { return predecessors_->size(); }
private:
  void SetPredecessor(StateId from, StateId to, Label label) {
    if (to >= predecessors_->size())
      predecessors_->resize(to + 1);
    PredecessorList &list = (*predecessors_)[to];
    PredecessorList::iterator i = list.find(from);
    if (i == list.end())
      i = list.insert(std::make_pair(from, empty_set_)).first;
    i->second.Add(label + offset_);
  }
  std::vector<ComposedTransducer::PredecessorList> *predecessors_;
  ContextSet empty_set_;
  Label offset_;
};


// =======================================================================

class StateObserver : public TransducerChangeObserver {
public:
  StateObserver(ComposedTransducer *receiver)
    : receiver_(receiver) {}
  virtual ~StateObserver() {}
  void NotifyAddState(const State *s) { receiver_->StateAdded(s); }
  void NotifyRemoveState(const State *s) { receiver_->StateRemoved(s); }
  void NotifyAddArc(const State::ArcRef) { receiver_->ArcUpdate(); }
  void NotifyRemoveArc(const State::ArcRef) { receiver_->ArcUpdate(); }
private:
  ComposedTransducer *receiver_;
};

// =======================================================================

ComposedTransducer::ComposedTransducer()
    : cfst_(NULL), lfst_(NULL), cl_(NULL),composed_states_(NULL),
      observer_(NULL), boundary_phone_(-1), num_phones_(0), num_states_(0),
      num_left_contexts_(0), center_sets_(false), need_update_(true) {}

ComposedTransducer::~ComposedTransducer() {
  delete lfst_;
  // composed_states_ are deleted by destructor of cl_
  delete cl_;
  delete cfst_;
  delete observer_;
}

void ComposedTransducer::SetCTransducer(ConstructionalTransducer *c) {
  CHECK_GE(boundary_phone_, 0);
  c_ = c;
  observer_ = new StateObserver(this);
  c_->RegisterObserver(observer_);
  num_phones_ = c_->NumPhones();
  num_left_contexts_ = c_->NumLeftContexts();
  center_sets_ = c_->HasCenterSets();
  cfst_ = new FstInterface();
  cfst_->Init(c_, boundary_phone_);
}

void ComposedTransducer::SetLTransducer(const fst::StdVectorFst &l) {
  lfst_ = new fst::StdVectorFst();
  fst::Map(l, lfst_, fst::ProjectMapper<fst::StdArc>(fst::PROJECT_INPUT));
  fst::RmEpsilon(lfst_, true);
  fst::ArcSort(lfst_, fst::StdILabelCompare());
  VLOG(1) << "pre-processed lexicon fst: #states: " << lfst_->NumStates();
  lfst_->SetInputSymbols(NULL);
}

void ComposedTransducer::Init() {
  Update();
}

// Update the ComposeFst, num_states_
void ComposedTransducer::Update() {
  // TODO(rybach): check if we can force the ComposeFst cache to forget
  //               everything and thereby keep cl_
  if (cl_) delete cl_;
  composed_states_ = new StateTable(*cfst_, *lfst_);
  typedef fst::ComposeFstOptions<fst::StdArc, Matcher,
                                 Filter, StateTable> Options;
  Options options(fst::CacheOptions(), 0, 0, 0, composed_states_);
  options.gc_limit = 0;
  cl_ = new fst::StdComposeFst(*cfst_, *lfst_, options);
  PredecessorVisitor<fst::StdArc> visitor(&cl_predecessors_, num_phones_, -1);
  fst::FifoQueue<fst::StdArc::StateId> queue;
  fst::Visit(*cl_, &visitor, &queue);
  num_states_ = visitor.NumStates();
  need_update_ = false;
}

int ComposedTransducer::NumStates() const {
  return num_states_;
}


void ComposedTransducer::ApplyModelSplit(
    int context_pos, const ContextQuestion *question, AllophoneModel *old_model,
    int hmm_state, const AllophoneModel::SplitResult &new_models) {
  c_->ApplyModelSplit(context_pos, question, old_model, hmm_state, new_models);
}

void ComposedTransducer::StateAdded(const State *s) {
  cfst_->AddState(s);
  need_update_ = true;
}

void ComposedTransducer::StateRemoved(const State *s) {
  // FstInterface::StateId id = cfst_->RemoveState(s);
  // composed_states_->EraseFirstState(id);
  cfst_->RemoveState(s);
  need_update_ = true;
}

void ComposedTransducer::FinishSplit() {
  c_->FinishSplit();
  if (need_update_) {
    cfst_->UpdateStartState();
    Update();
  }
}

AbstractSplitPredictor* ComposedTransducer::CreateSplitPredictor() const {
  return new ComposedStatePredictor(*this);
}

}  // namespace trainc
