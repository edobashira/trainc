// context_builder.cc
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
#include <ext/hash_set>
#include <limits>
#include <set>
#include "fst/symbol-table.h"
#include "fst/vector-fst.h"
#include "file.h"
#include "stringutil.h"
#include "util.h"
#include "composed_transducer.h"
#include "context_set.h"
#include "hash.h"
#include "hmm_compiler.h"
#include "lexicon_check.h"
#include "lexicon_compiler.h"
#include "lexicon_transducer.h"
#include "model_splitter.h"
#include "phone_models.h"
#include "recipe.h"
#include "sample.h"
#include "set_inventory.h"
#include "scorer.h"
#include "transducer.h"
#include "transducer_check.h"
#include "transducer_compiler.h"
#include "transducer_init.h"
#include "context_builder.h"

namespace trainc {

using __gnu_cxx::hash_set;
using fst::SymbolTable;


ContextBuilder::ContextBuilder()
    : phone_symbols_(NULL),
      num_phones_(-1),
      phone_info_(NULL),
      all_phones_(NULL),
      transducer_init_("basic"),
      use_composition_(true),
      transducer_(NULL),
      cl_transducer_(NULL),
      hmm_compiler_(NULL),
      models_(NULL),
      scorer_(NULL),
      boundary_phone_(-1),
      num_left_contexts_(-1),
      num_right_contexts_(-1),
      split_center_(false),
      shifted_cl_(true),
      determistic_cl_(true),
      builder_(new ModelSplitter()) {}

ContextBuilder::~ContextBuilder() {
  delete phone_symbols_;
  delete phone_info_;
  delete all_phones_;
  delete cl_transducer_;
  delete transducer_;
  delete hmm_compiler_;
  delete models_;
  delete scorer_;
  while (!question_sets_.empty()) {
    STLDeleteElements(&question_sets_.back());
    question_sets_.pop_back();
  }
  delete builder_;
}

void ContextBuilder::SetReplay(const std::string &filename) {
  if (!filename.empty()) {
    delete builder_;
    VLOG(1) << "using split file " << filename;
    File *file = File::OpenOrDie(filename, "r");
    ReplaySplitter *splitter = new ReplaySplitter();
    if (!splitter->SetFile(file))
      LOG(FATAL) << "error reading split file " << filename;
    builder_ = splitter;
  }
}

void ContextBuilder::SetSaveSplits(const std::string &filename) {
  if (!filename.empty()) {
    File *file = File::OpenOrDie(filename, "w");
    builder_->SetRecipeWriter(file);
  }
}

// Set num_phones_, construct all_phones, and create phone_info_
void ContextBuilder::SetPhoneSymbols(const SymbolTable &phone_symbols) {
  delete phone_symbols_;
  phone_symbols_ = phone_symbols.Copy();
  // don't count epsilon as phone
  num_phones_ = phone_symbols_->AvailableKey() - 1;
  all_phones_ = new ContextSet(num_phones_);
  for (int i = 0; i < num_phones_; ++i) {
    all_phones_->Add(i);
  }
  phone_info_ = new Phones(num_phones_);
  builder_->SetPhoneInfo(phone_info_);
  builder_->SetPhoneSymbols(phone_symbols_);
}

// Convert the set of CI phones to the format used in Phones.
void ContextBuilder::SetCiPhones(const set<int> &ci_phones) {
  for (set<int>::const_iterator p = ci_phones.begin();
      p != ci_phones.end(); ++p) {
    CHECK_GT(*p, 0);
    phone_info_->SetCiPhone(*p - 1);
  }
}

void ContextBuilder::SetBoundaryPhone(const string &phone_name) {
  CHECK_GT(num_phones_, 0);
  // TODO(rybach): need a index shift here? inconsistent phone representation.
  boundary_phone_ = phone_symbols_->Find(phone_name) - 1;
  CHECK_GT(boundary_phone_, 0);
}

// Convert set of questions to a set of ContextQuestion objects.
void ContextBuilder::SetDefaultQuestionSet(
    const SetInventory &question_set) {
  CHECK(question_sets_.empty());
  CHECK(builder_->GetQuestions()->empty());
  CHECK_GE(num_left_contexts_, 0);
  CHECK_GE(num_right_contexts_, 0);
  question_sets_.push_back(QuestionSet());
  QuestionSet *default_questions = &question_sets_.back();
  ConvertQuestionSet(question_set, default_questions);
  builder_->GetQuestions()->resize(num_left_contexts_ + num_right_contexts_ + 1,
                                   default_questions);
}

void ContextBuilder::SetQuestionSetPerContext(
    int context_position, const SetInventory &question_set) {
  vector<const QuestionSet*> &questions = *builder_->GetQuestions();
  CHECK(!questions.empty());
  question_sets_.push_back(QuestionSet());
  QuestionSet *qs = &question_sets_.back();
  ConvertQuestionSet(question_set, qs);
  int pos = context_position + num_left_contexts_;
  CHECK(pos < questions.size());
  questions[pos] = qs;
}

void ContextBuilder::ConvertQuestionSet(
    const SetInventory &set_inventory,
    QuestionSet *question_set) const {
  CHECK_GT(num_phones_, 0);
  CHECK_EQ(phone_symbols_->NumSymbols(),
           set_inventory.GetSymTable()->NumSymbols());
  InventoryIterator inv_iter(&set_inventory);
  hash_set<ContextSet, Hash<ContextSet>, Equal<ContextSet> > sets;
  for (; !inv_iter.Done(); inv_iter.Next()) {
    ContextSetIterator cs_iter(inv_iter.Value());
    ContextSet phone_set(num_phones_);
    for (; !cs_iter.Done(); cs_iter.Next()) {
      CHECK_GT(cs_iter.Value(), 0);
      phone_set.Add(cs_iter.Value() - 1);  // offset -1 !
    }
    if (!sets.count(phone_set)) {
      question_set->push_back(new ContextQuestion(phone_set, inv_iter.Name()));
      sets.insert(phone_set);
    } else {
      LOG(WARNING) << "ignoring redundant question " << inv_iter.Name();
    }
  }
}

void ContextBuilder::SetContextLength(int left_context, int right_context,
                                      bool split_center) {

  num_left_contexts_ = left_context;
  num_right_contexts_ = right_context;
  split_center_ = split_center;
  builder_->SetContext(num_left_contexts_, num_right_contexts_, split_center_);
  if (num_right_contexts_ > 1) {
    REP(FATAL) << "currently a maximum right context size "
                  "of 1 is supported";
  }
}

void ContextBuilder::SetMinSplitGain(float min_gain) {
  builder_->SetMinGain(min_gain);
}

void ContextBuilder::SetMinSeenContexts(int num_contexts) {
  builder_->SetMinContexts(num_contexts);
}

void ContextBuilder::SetMinObservations(int num_obs) {
  builder_->SetMinObservations(num_obs);
}

void ContextBuilder::SetVarianceFloor(float floor) {
  variance_floor_ = floor;
}

void ContextBuilder::SetTargetNumModels(int num_models) {
  builder_->SetTargetNumModels(num_models);
}

void ContextBuilder::SetTargetNumStates(int num_states) {
  builder_->SetTargetNumStates(num_states);
}

void ContextBuilder::SetMaxHypotheses(int max_hyps) {
  builder_->SetMaxHypotheses(max_hyps);
}

void ContextBuilder::SetStatePenaltyWeight(float weight) {
  builder_->SetStatePenaltyWeight(weight);
}

void ContextBuilder::SetIgnoreAbsentModels(bool ignore) {
  builder_->SetIgnoreAbsentModels(ignore);
}

// Store number of HMM states in phone_info_
void ContextBuilder::SetPhoneLength(int phone, int num_states) {
  if (phone == 0) {
    // ignore epsilon phone symbol
    LOG(WARNING) << "SetPhoneLength for phone=0: " << num_states;
    return;
  }
  if (num_states == 0) {
    LOG(WARNING) << "phone length 0 for phone " << phone
              << " = " << phone_symbols_->Find(phone);
  }
  phone_info_->SetPhoneLength(phone - 1, num_states);
}

// read number of states per phone from file
void ContextBuilder::SetPhoneLength(const std::string &filename) {
  File *file = File::OpenOrDie(filename, "r");
  InputBuffer rbuf(file);
  string line;
  while (rbuf.ReadLine(&line)) {
    if (line.empty()) continue;
    vector<string> items;
    SplitStringUsing(line, " ", &items);
    CHECK_EQ(items.size(), 2);
    int phone = phone_symbols_->Find(items[0]);
    CHECK_GE(phone, 0);
    int num_states = std::atoi(items[1].c_str());
    CHECK_GT(num_states, 0);
    SetPhoneLength(phone, num_states);
  }
}

void ContextBuilder::SetPhoneMapping(const string &filename) {
  CHECK(phone_symbols_);
  CHECK(phone_info_);
  File *file = File::OpenOrDie(filename, "r");
  InputBuffer rbuf(file);
  string line;
  while (rbuf.ReadLine(&line)) {
    if (line.empty()) continue;
    vector<string> items;
    SplitStringUsing(line, " ", &items);
    CHECK_EQ(items.size(), 2);
    VLOG(2) << "mapping " << items[0] << " to " << items[1];
    int key = phone_symbols_->Find(items[0]) - 1;
    int value = phone_symbols_->Find(items[1]) - 1;
    CHECK_GE(key, 0);
    CHECK_GE(value, 0);
    phone_mapping_[key] = value;
    if (phone_info_->IsCiPhone(value) ^ phone_info_->IsCiPhone(key)) {
      REP(FATAL) << "cannot map CD phone " << items[0]
                 << " to CI phone " << items[1];
    }
  }
  CHECK_GT(phone_mapping_.size(), 0);
}

void ContextBuilder::SetTransducerInitType(const string &init_type) {
  transducer_init_ = init_type;
}

void ContextBuilder::SetCountingTransducer(const string &filename) {
  counting_transducer_file_ = filename;
}

void ContextBuilder::SetUseComposition(bool use_composition) {
  use_composition_ = use_composition;
}

void ContextBuilder::SetShiftedTransducer(bool shifted) {
  shifted_cl_ = shifted;
}

void ContextBuilder::SetSplitDetermistic(bool split_deterministic) {
  determistic_cl_ = split_deterministic;
}

// Convert phone from string representation to internal index.
void ContextBuilder::ConvertPhones(
    const vector<string> &src, vector<int> *dst) const {
  for (vector<string>::const_iterator s = src.begin(); s != src.end(); ++s) {
    if (s->empty()) continue;
    int p = phone_symbols_->Find(*s);
    if (p <= 0) {
      REP(FATAL) << "unknown phone symbol: " << *s;
    }
    dst->push_back(p - 1);
  }
}

// Read phones symbols from file, one symbol per line and
// convert them to the internal index.
void ContextBuilder::ConvertPhonesFromFile(
    const string &filename, vector<int> *dst) const {
  string buffer;
  File::ReadFileToStringOrDie(filename, &buffer);
  vector<string> phones;
  SplitStringUsing(buffer, "\n", &phones);
  ConvertPhones(phones, dst);
}

void ContextBuilder::SetInitialPhones(const string &filename) {
  ConvertPhonesFromFile(filename, &initial_phones_);
}

void ContextBuilder::SetFinalPhones(const string &filename) {
  ConvertPhonesFromFile(filename, &final_phones_);
}

void ContextBuilder::SetInitialPhones(const vector<string> &initial_phones) {
  ConvertPhones(initial_phones, &initial_phones_);
}

void ContextBuilder::SetFinalPhones(const vector<string> &final_phones) {
  ConvertPhones(final_phones, &final_phones_);
}

void ContextBuilder::SetSamples(const Samples *samples) {
  builder_->SetSamples(samples);
}

// Perform a structural check on the ConstructionalTransducer.
bool ContextBuilder::CheckTransducer() const {
  return ConstructionalTransducerCheck(
        *transducer_, phone_info_, num_left_contexts_,
        num_right_contexts_).IsValid();
}

// Create and initialize the ConstructionalTransducer and the
// initial monophone state models.
ConstructionalTransducer* ContextBuilder::CreateTransducer(
    ModelManager *models) const {
  ConstructionalTransducer *t = new ConstructionalTransducer(
      num_phones_, num_left_contexts_, num_right_contexts_,
      split_center_);
  TransducerInitialization *initT = TransducerInitializationFactory::Create(
      transducer_init_, phone_mapping_, initial_phones_, final_phones_);
  if (!initT) {
    REP(FATAL) << "transducer initialization failed. "
               << "unknown initialization method?";
  }
  initT->SetPhoneInfo(phone_info_);
  initT->SetContextLenghts(num_left_contexts_, num_right_contexts_);
  initT->SetAnyPhoneContext(all_phones_);
  CHECK(initT->Prepare());
  initT->CreateModels(models);
  initT->Execute(t);
  delete initT;
  return t;
}

ComposedTransducer* ContextBuilder::CreateComposedTransducer(
    const string &l_file, ConstructionalTransducer *c) const
{
  ComposedTransducer *cl = new ComposedTransducer();
  cl->SetBoundaryPhone(boundary_phone_);
  cl->SetCTransducer(c);
  VLOG(1) << "using L tranducer: " << l_file;
  fst::StdVectorFst *l = fst::StdVectorFst::Read(l_file);
  CHECK_NOTNULL(l);
  VLOG(1) << "# of states: " << l->NumStates();
  cl->SetLTransducer(*l);
  cl->Init();
  delete l;
  return cl;
}

LexiconTransducer* ContextBuilder::CreateLexiconTransducer(
    const string &l_file, ConstructionalTransducer *c) const
{
  LexiconTransducer *cl = new LexiconTransducer();
  cl->SetShifted(shifted_cl_);
  if (!shifted_cl_)
    cl->SetSplitDerministic(determistic_cl_);
  cl->SetCTransducer(c);
  VLOG(1) << "using L transducer: " << l_file;
  fst::StdVectorFst *l = fst::StdVectorFst::Read(l_file);
  CHECK_NOTNULL(l);
  cl->Init(*l, *models_, phone_mapping_, boundary_phone_);
  delete l;
  return cl;
}

void ContextBuilder::Build() {
  CHECK_GT(num_phones_, 0);
  models_ = new ModelManager();
  scorer_ = new MaximumLikelihoodScorer(variance_floor_);
  // TODO(rybach): add scorer factory or at least a SetScorer method
  builder_->SetScorer(scorer_);
  transducer_ = CreateTransducer(models_);
  StateCountingTransducer *count_transducer = transducer_;
  ComposedTransducer *cl = NULL;
  if (!counting_transducer_file_.empty()) {
    if (use_composition_) {
      count_transducer = cl = CreateComposedTransducer(
          counting_transducer_file_, transducer_);
      VLOG(1) << "using composed transducer";
    } else {
      count_transducer = cl_transducer_ = CreateLexiconTransducer(
          counting_transducer_file_, transducer_);
      VLOG(1) << "using counting transducer directly";
    }
  }
  builder_->SetTransducer(count_transducer);
  builder_->InitModels(models_);
  builder_->InitSplitHypotheses(models_);
  builder_->SplitModels(models_);
  builder_->Cleanup();
  if (!CheckTransducer()) {
    LOG(WARNING) << "C transducer seems to be invalid";
  }
  delete cl;
  hmm_compiler_ = new HmmCompiler();
  hmm_compiler_->SetModels(models_);
  hmm_compiler_->SetPhoneInfo(phone_info_);
  hmm_compiler_->SetPhoneSymbols(phone_symbols_);
  hmm_compiler_->SetVarianceFloor(variance_floor_);
  hmm_compiler_->EnumerateModels();
}

void ContextBuilder::WriteStateInfo(const string &filename) const {
  File *file = File::OpenOrDie(filename, "w");
  OutputBuffer obuf(file);
  for (StateIterator si(*transducer_); !si.Done(); si.Next()) {
    const State &state = si.Value();
    string h;
    for (int c = 1; c <= state.history().NumLeftContexts(); ++c) {
      string cs = "{";
      const ContextSet &set = state.GetHistory(-c);
      for (int p = 0; p < num_phones_; ++p) {
        if (set.HasElement(p)) cs += phone_symbols_->Find(p + 1) + ",";
      }
      cs += "}";
      h += cs + " ";
    }
    string phones = "";
    for (ContextSet::Iterator p(state.center()); !p.Done(); p.Next()) {
        phones += phone_symbols_->Find(p.Value() + 1) + " ";
    }
    obuf.WriteString(StringPrintf("[%s] %s\n", phones.c_str(), h.c_str()));
  }
  if (!obuf.CloseFile())
    REP(FATAL) << "Close failed for " << filename;
}

const HmmCompiler& ContextBuilder::GetHmmCompiler() const {
  CHECK_NOTNULL(hmm_compiler_);
  return *hmm_compiler_;
}

void ContextBuilder::WriteTransducer(const string &filename) const {
  CHECK_NOTNULL(hmm_compiler_);
  CHECK_NOTNULL(transducer_);
  HmmTransducerCompiler compiler;
  compiler.SetBoundaryPhone(boundary_phone_);
  compiler.SetHmmCompiler(hmm_compiler_);
  compiler.SetTransducer(transducer_);
  fst::StdVectorFst *c = compiler.CreateTransducer();
  c->Write(filename);
  VLOG(1) << "wrote " << filename;
  delete c;
}

void ContextBuilder::WriteCountingTransducer(const string &filename) const {
  if (!cl_transducer_) {
    LOG(WARNING) << "cannot create split counting transducer";
    return;
  }
  CHECK_NOTNULL(hmm_compiler_);
  {
    LexiconTransducerCheck check(phone_info_);
    check.SetTransducer(cl_transducer_);
    if (!check.IsValid()) {
      LOG(WARNING) << "counting transducer seems to be invalid";
    }
  }
  LexiconTransducerCompiler compiler;
  compiler.SetBoundaryPhone(boundary_phone_);
  compiler.SetHmmCompiler(hmm_compiler_);
  compiler.SetTransducer(cl_transducer_);
  fst::StdVectorFst *cl = compiler.CreateTransducer();
  cl->Write(filename);
  VLOG(1) << "wrote " << filename;
  delete cl;
}

int ContextBuilder::NumStates() const {
  return transducer_->NumStates();
}

}  // namespace trainc
