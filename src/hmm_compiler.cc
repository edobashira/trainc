// hmm_compiler.cc
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

#include <algorithm>
#include <ext/functional>
#include "fst/symbol-table.h"
#include "fst/vector-fst.h"
#include "file.h"
#include "stringutil.h"
#include "phone_models.h"
#include "gaussian_model.h"
#include "hmm_compiler.h"
#include "util.h"

using __gnu_cxx::select1st;
using namespace std;
using fst::StdArc;
using fst::StdVectorFst;


namespace trainc {

const char *HmmCompiler::kHmmStateSymbolsName = "CDStates";
const char *HmmCompiler::kHmmSymbolsName = "CDHMMs";

HmmCompiler::HmmCompiler()
    : models_(NULL),
      phone_info_(NULL),
      phone_symbols_(NULL),
      hmm_state_symbols_(NULL),
      hmm_symbols_(NULL),
      model_(NULL),
      variance_floor_(0.001),
      state_model_index_(0) {}

HmmCompiler::~HmmCompiler() {
  delete hmm_state_symbols_;
  delete hmm_symbols_;
  delete model_;
}

void HmmCompiler::InitStateModelIndex() {
  state_model_index_.clear();
  state_model_index_.resize(phone_info_->NumPhones());
  for (int p = 0; p < phone_info_->NumPhones(); ++p) {
    int np = std::max(phone_info_->NumHmmStates(p), 0);
    state_model_index_[p].resize(np, 1);
  }
}

void HmmCompiler::AddPhoneModel(const AllophoneModel *phone_model) {
  PhoneModelMap::const_iterator i = phone_models_.find(phone_model);
  if (i == phone_models_.end()) {
    int index = next_hmm_index_++;
    phone_models_.insert(make_pair(phone_model, index));
  }
  hmm_symbols_->AddSymbol(GetHmmName(phone_model));
}

string HmmCompiler::GetHmmName(const AllophoneModel *phone_model) const {
  PhoneModelMap::const_iterator i = phone_models_.find(phone_model);
  CHECK(i != phone_models_.end());
  CHECK(!phone_model->phones().empty());
  string phone = phone_symbols_->Find(phone_model->phones().front() + 1);
  return StringPrintf("%s_%d", phone.c_str(), i->second);
}

string HmmCompiler::GetHmmStateName(
    const AllophoneStateModel *state_model) const {
  DCHECK_GT(state_model->GetAllophones().size(), 0);
  int phone = state_model->GetAllophones().front()->phones().front();
  int hmm_state = state_model->state();
  string phone_symbol = phone_symbols_->Find(phone + 1);
  return StringPrintf("%s_%d", phone_symbol.c_str(), hmm_state + 1);
}

const fst::SymbolTable& HmmCompiler::GetHmmSymbols() const {
  CHECK_NOTNULL(hmm_symbols_);
  return *hmm_symbols_;
}

void HmmCompiler::AddStateModel(const AllophoneStateModel *state_model) {
  string state_name = GetHmmStateName(state_model);
  CHECK_GT(state_model->GetAllophones().size(), 0);
  int phone = state_model->GetAllophones().front()->phones().front();
  int hmm_state = state_model->state();
  int index = state_model_index_[phone][hmm_state]++;
  string state_model_name = StringPrintf("%s.%d", state_name.c_str(), index);
  CHECK(state_models_.find(state_model) == state_models_.end());
  state_models_.insert(make_pair(state_model, state_model_name));
}

void HmmCompiler::InitSymbols() {
  hmm_state_symbols_ = new fst::SymbolTable(kHmmStateSymbolsName);
  hmm_state_symbols_->AddSymbol(".eps", 0);
  hmm_state_symbols_->AddSymbol(".wb", 1);
  hmm_symbols_ = new fst::SymbolTable(kHmmSymbolsName);
  hmm_symbols_->AddSymbol(".eps", 0);
  hmm_symbols_->AddSymbol(".wb", 1);
}

void HmmCompiler::EnumerateModels() {
  InitStateModelIndex();
  InitSymbols();
  next_hmm_index_ = 1;
  const ModelManager::StateModelList model_list = models_->GetStateModels();
  typedef ModelManager::StateModelList::const_iterator ModelIter;
  typedef AllophoneStateModel::AllophoneRefList AllophoneList;
  for (ModelIter mi = model_list.begin(); mi != model_list.end(); ++mi) {
    const AllophoneStateModel* state_model = *mi;
    AddStateModel(state_model);
    const AllophoneList &allophones = state_model->GetAllophones();
    for (AllophoneList::const_iterator ai = allophones.begin();
        ai != allophones.end(); ++ai) {
      const AllophoneModel *allophone = *ai;
      AddPhoneModel(allophone);
    }
  }
  state_model_index_.clear();
  REP(INFO) << "Number of unique HMMs: " << phone_models_.size();
  REP(INFO) << "Number of HMM state models: " << state_models_.size();
  CreateStateModels();
}

namespace {
  typedef hash_map<const AllophoneStateModel*, string,
                   PointerHash<const AllophoneStateModel> > StateModelMap;
  // Compare the names of the state models.
  // TODO(rybach): sorting using hash map lookups is expensive,
  //               sort hash map items instead.
  class StateModelNameCompare {
   public:
    explicit StateModelNameCompare(const StateModelMap &name_map)
        : name_map_(name_map) {}
    bool operator()(const AllophoneStateModel *a,
                    const AllophoneStateModel *b) const {
      return name_map_.find(a)->second < name_map_.find(b)->second;
    }
   private:
    const StateModelMap &name_map_;
  };

  typedef hash_map<const AllophoneModel*, int,
                   PointerHash<const AllophoneModel> > PhoneModelMap;
  // Compare AllophoneModels by index.
  // TODO(rybach): sorting using hash map lookups is expensive,
  //               sort hash map items instead.
  class PhoneModelIndexCompare {
   public:
    explicit PhoneModelIndexCompare(const PhoneModelMap &index_map)
        : index_map_(index_map) {}
    bool operator()(const AllophoneModel *a, const AllophoneModel *b) const {
      return index_map_.find(a)->second < index_map_.find(b)->second;
    }
   private:
    const PhoneModelMap &index_map_;
  };
}  // namespace

// Create the state models and the HMM state symbols.
// The index of a state model in the GaussianModel model_ and its index
// its name in the of the HMM state name symbol table hmm_state_symbols_
// must be the same (with an offset of 2 for epsilon and word boundary
// symbol). Furthermore, the state models have to be sorted by their name,
// because other steps in AM training assume them to be sorted.
void HmmCompiler::CreateStateModels() {
  CHECK_GT(state_models_.size(), 0);
  CHECK_EQ(hmm_state_symbols_->AvailableKey(), 2);
  CHECK(model_ == NULL);
  int state_model_index = 0;
  model_ = new GaussianModel();
  // sort the state models lexicographically by name.
  vector<StateModelMap::key_type> sorted_models(state_models_.size());
  transform(state_models_.begin(), state_models_.end(), sorted_models.begin(),
            select1st<StateModelMap::value_type>());
  sort(sorted_models.begin(), sorted_models.end(),
       StateModelNameCompare(state_models_));
  typedef vector<StateModelMap::key_type>::const_iterator ModelIter;
  for (ModelIter si = sorted_models.begin(); si != sorted_models.end(); ++si) {
    const AllophoneStateModel *state_model = *si;
    const string model_name = state_models_[state_model];
    VLOG(3) << "state model " << state_model << " " << model_name;
    VLOG(3) << " num_obs=" << state_model->NumObservations();
    state_model->AddToModel(model_name, model_, variance_floor_);
    CHECK_EQ(hmm_state_symbols_->AddSymbol(model_name), state_model_index + 2);
    ++state_model_index;
  }
}

void HmmCompiler::WriteHmmList(const string &filename) const {
  CHECK_GT(phone_models_.size(), 0);
  File *file = File::OpenOrDie(filename, "w");
  OutputBuffer obuf(file);
  obuf.WriteString(".eps\n.wb\n");
  vector<PhoneModelMap::key_type> sorted_models(phone_models_.size());
  transform(phone_models_.begin(), phone_models_.end(), sorted_models.begin(),
            select1st<PhoneModelMap::value_type>());
  sort(sorted_models.begin(), sorted_models.end(),
       PhoneModelIndexCompare(phone_models_));
  typedef vector<PhoneModelMap::key_type>::const_iterator ModelIter;
  for (ModelIter ai = sorted_models.begin(); ai != sorted_models.end(); ++ai) {
    const AllophoneModel *phone_model = *ai;
    obuf.WriteString(GetHmmName(phone_model));
    for (int s = 0; s < phone_model->NumStates(); ++s) {
      const AllophoneStateModel *state_model = phone_model->GetStateModel(s);
      StateModelMap::const_iterator sm = state_models_.find(state_model);
      CHECK(sm != state_models_.end());
      obuf.WriteString(" " + sm->second);
    }
    obuf.WriteString("\n");
  }
  if (!obuf.CloseFile())
    REP(FATAL) << "Close failed for hmm list " << filename;
}

void HmmCompiler::WriteStateModels(
    const string &filename, const string &file_type,
    const string &feature_type,
    const string &frontend_config) const {
  CHECK_NOTNULL(model_);
  ModelWriter *writer = ModelWriter::Create(file_type);
  model_->SetFrontendDescription(frontend_config);
  model_->SetFeatureDescription(feature_type);
  if (!writer->Write(filename, *model_))
    REP(FATAL) << "Cannot write " << filename;
  delete writer;
}

void HmmCompiler::WriteStateSymbols(const string &filename) const {
  CHECK_NOTNULL(hmm_state_symbols_);
  hmm_state_symbols_->WriteText(filename);
}

void HmmCompiler::WriteHmmSymbols(const string &filename) const {
  CHECK_NOTNULL(hmm_symbols_);
  CHECK_NOTNULL(hmm_state_symbols_);
  hmm_symbols_->WriteText(filename);
}

void HmmCompiler::WriteCDHMMtoPhoneMap(const string &filename) const {
  CHECK_GT(phone_models_.size(), 0);
  File *file = File::OpenOrDie(filename, "w");
  OutputBuffer obuf(file);
  obuf.WriteString(".eps .eps\n.wb .wb\n");
  for (PhoneModelMap::const_iterator p = phone_models_.begin();
      p != phone_models_.end(); ++p) {
    const AllophoneModel *model = p->first;
    const string phone_symbol =
        phone_symbols_->Find(model->phones().front() + 1);
    obuf.WriteString(StringPrintf("%s %s\n",
                                  GetHmmName(model).c_str(),
                                  phone_symbol.c_str()));
  }
  if (!obuf.CloseFile())
    REP(FATAL) << "Close failed for " << filename;
}

void HmmCompiler::WriteStateNameMap(const string &filename) const {
  CHECK_GT(state_models_.size(), 0);
  File *file = File::OpenOrDie(filename, "w");
  OutputBuffer obuf(file);
  for (StateModelMap::const_iterator s = state_models_.begin();
      s != state_models_.end(); ++s) {
    string state_name = GetHmmStateName(s->first);
    obuf.WriteString(StringPrintf("%s %s\n",
                                  s->second.c_str(), state_name.c_str()));
  }
  if (!obuf.CloseFile())
    REP(FATAL) << "Close failed for " << filename;
}

// TODO(rybach): Create H transducer using the topology transducer.
// TODO(rybach): Create deterministic transducer?
void HmmCompiler::WriteHmmTransducer(const string &filename) const {
  CHECK_GT(phone_models_.size(), 0);
  StdVectorFst h;
  CHECK_EQ(h.AddState(), 0);
  h.SetStart(0);
  h.SetFinal(0, StdArc::Weight::One());
  for (PhoneModelMap::const_iterator p = phone_models_.begin();
      p != phone_models_.end(); ++p) {
    const AllophoneModel *phone_model = p->first;
    StdArc::Label output = hmm_symbols_->Find(GetHmmName(phone_model));
    StdArc::StateId state = h.Start(), next_state = fst::kNoStateId;
    int num_hmm_states = phone_model->NumStates();
    for (int s = 0; s < num_hmm_states; ++s) {
      const AllophoneStateModel *state_model = phone_model->GetStateModel(s);
      StateModelMap::const_iterator sm = state_models_.find(state_model);
      StdArc::Label input = hmm_state_symbols_->Find(sm->second);
      if (s < (num_hmm_states - 1))
        next_state = h.AddState();
      else
        next_state = h.Start();
      StdArc new_arc(input, output, StdArc::Weight::One(), next_state);
      h.AddArc(state, new_arc);
      state = next_state;
      output = 0;  // epsilon
    }
  }
  h.Write(filename);
}

void HmmCompiler::WriteStateModelInfo(const string &filename) const {
  CHECK_GT(state_models_.size(), 0);
  File *file = File::OpenOrDie(filename, "w");
  OutputBuffer obuf(file);
  for (StateModelMap::const_iterator s = state_models_.begin();
      s != state_models_.end(); ++s) {
    const AllophoneStateModel *state_model = s->first;
    const PhoneContext &context = state_model->GetContext();
    string sets;
    for (int pos = -context.NumLeftContexts();
         pos <= context.NumRightContexts(); ++pos) {
      sets += StringPrintf("%d={", pos);
      const ContextSet &c = context.GetContext(pos);
      for (ContextSet::Iterator p(c); !p.Done(); p.Next())
        sets += phone_symbols_->Find(p.Value() + 1) + " ";
      sets += "} ";
    }
    obuf.WriteString(s->second);
    obuf.WriteString(StringPrintf(" num_obs=%d",
                                  state_model->NumObservations()));
    obuf.WriteString(StringPrintf(" num_context=%d",
                                  state_model->NumSeenContexts()));
    obuf.WriteString(StringPrintf(" cost=%f ",
                                  state_model->GetCost()));
    obuf.WriteString(sets);
    obuf.WriteString("\n");
  }
  if (!obuf.CloseFile())
    REP(FATAL) << "Close failed for " << filename;
}

namespace {
struct PhonePair : public pair<int, int> {
  PhonePair(int i, int j) : pair<int,int>(i,j) {}
  size_t HashValue() const {
    return first << 16 | second;
  }
};
}

void HmmCompiler::WriteNonDetC(const std::string &filename,
                               int boundary_phone) const {
  typedef fst::StdArc::StateId StateId;
  typedef hash_map<PhonePair, StateId, Hash<PhonePair> > StateMap;
  StateMap states;
  fst::StdVectorFst ct;
  StateId start = ct.AddState();
  ct.SetStart(start);
  for (PhoneModelMap::const_iterator p = phone_models_.begin(); p
      != phone_models_.end(); ++p) {
    const AllophoneModel *m = p->first;
    string input_symbol = GetHmmName(m);
    fst::StdArc::Label input = hmm_symbols_->Find(GetHmmName(m));
    ContextSet left(0), right(0);
    m->GetCommonContext(-1, &left);
    m->GetCommonContext(1, &right);
    if (left.IsEmpty() && right.IsEmpty()) {
      left.Invert();
      right.Invert();
    }
    for (vector<int>::const_iterator c = m->phones().begin(); c
        != m->phones().end(); ++c) {
      for (ContextSet::Iterator l(left); !l.Done(); l.Next()) {
        PhonePair pfrom(l.Value(), *c);
        StateMap::const_iterator i = states.find(pfrom);
        if (i == states.end())
          states[pfrom] = ct.AddState();
        StateId sfrom = states[pfrom];
        for (ContextSet::Iterator r(right); !r.Done(); r.Next()) {
          PhonePair pto(*c, r.Value());
          StateMap::const_iterator j = states.find(pto);
          if (j == states.end())
            states[pto] = ct.AddState();
          StateId sto = states[pto];
          if (l.Value() == boundary_phone) {
            ct.AddArc(start, fst::StdArc(input, *c + 1,
                fst::StdArc::Weight::One(), sto));
          }
          if (r.Value() == boundary_phone) {
            ct.SetFinal(sto, fst::StdArc::Weight::One());
          }
          ct.AddArc(sfrom, fst::StdArc(input, *c + 1,
              fst::StdArc::Weight::One(), sto));
        }
      }
    }
  }
  ct.SetInputSymbols(hmm_symbols_);
  ct.SetOutputSymbols(phone_symbols_);
  ct.Write(filename);
}

vector<const AllophoneStateModel*> HmmCompiler::GetStateModels(
    int phone, int hmm_state) const {
  CHECK_GT(state_models_.size(), 0);
  int internal_phone = phone - 1;
  CHECK_GE(internal_phone, 0);
  vector<const AllophoneStateModel*> result;
  for (StateModelMap::const_iterator s = state_models_.begin();
      s != state_models_.end(); ++s) {
    const AllophoneStateModel *state_model = s->first;
    const AllophoneModel *phone_model = state_model->GetAllophones().front();
    if (phone_model->phones().front() == internal_phone
        && state_model->state() == hmm_state) {
      result.push_back(state_model);
    }
  }
  return result;
}

int HmmCompiler::NumStateModels() const {
  return state_models_.size();
}

int HmmCompiler::NumHmmModels() const {
  return phone_models_.size();
}

}  // namespace trainc
