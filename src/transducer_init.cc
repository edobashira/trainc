// transducer_init.cc
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
#include <ext/numeric>
#include "transducer_init.h"
#include "phone_models.h"
#include "transducer.h"
#include "util.h"

using __gnu_cxx::iota;

namespace trainc {

void BasicTransducerInitialization::CreateModels(ModelManager *models) {
  int num_phones = phone_info_->NumPhones();
  PhoneContext empty_context(num_phones, num_left_contexts_,
                            num_right_contexts_);
  PhoneContext any_context(num_phones, num_left_contexts_,
                           num_right_contexts_);
  for (int pos = -num_left_contexts_; pos <= num_right_contexts_; ++pos) {
    if (pos) any_context.SetContext(pos, *any_phone_);
  }
  phone_models_.resize(num_phones, NULL);
  for (int phone = 0; phone < num_phones; ++phone) {
    CreatePhoneModel(models, phone, (phone_info_->IsCiPhone(phone) ?
                                     empty_context : any_context));
    VLOG(2) << phone_models_[phone]->ToString(true);
  }
  CreateUnits(num_phones);
}

// Fill units_ with a list of unique symbols.
void BasicTransducerInitialization::CreateUnits(int num_phones) {
  // no mapping, all phones are separate units
  units_.resize(num_phones);
  iota(units_.begin(), units_.end(), 0);
}

void BasicTransducerInitialization::SetUnitHistory(
    int phone, PhoneContext *history) const {
  CHECK(history->GetContext(0).IsEmpty());
  history->GetContextRef(0)->Add(phone);
}

void BasicTransducerInitialization::CreatePhoneModel(
    ModelManager *models, int phone, const PhoneContext &context) {
  PhoneContext phone_context = context;
  CHECK(phone_context.GetContext(0).IsEmpty());
  phone_context.GetContextRef(0)->Add(phone);
  phone_models_[phone] = models->InitAllophoneModel(
      phone, phone_info_->NumHmmStates(phone), phone_context);
}

void BasicTransducerInitialization::CreateStates(ConstructionalTransducer *t) {
  int num_phones = phone_info_->NumPhones();
  PhoneContext ci_history(num_phones, num_left_contexts_, 0);
  PhoneContext any_history(num_phones, num_left_contexts_, 0);
  for (int i = 1; i <= num_left_contexts_; ++i) {
    any_history.SetContext(-i, *any_phone_);
    if (i < num_left_contexts_) ci_history.SetContext(-i, *any_phone_);
  }
  phone_states_.resize(num_phones, NULL);
  for (vector<int>::const_iterator ui = units_.begin();
      ui != units_.end(); ++ui) {
    int p = *ui;
    PhoneContext state_history =
        (phone_info_->IsCiPhone(p) ? ci_history : any_history);
    SetUnitHistory(p, &state_history);
    phone_states_[p] = t->AddState(state_history);
  }
}

void BasicTransducerInitialization::CreateArcs(ConstructionalTransducer *t) {
  int num_phones = phone_info_->NumPhones();
  for (vector<int>::const_iterator src_unit = units_.begin();
      src_unit != units_.end(); ++src_unit) {
    int src_phone = *src_unit;
    CHECK_NOTNULL(phone_states_[src_phone]);
    for (int next_phone = 0; next_phone < num_phones; ++next_phone) {
      CHECK_NOTNULL(phone_states_[next_phone]);
      t->AddArc(phone_states_[src_phone], phone_states_[next_phone],
                phone_models_[src_phone], next_phone);
    }
  }
}

void BasicTransducerInitialization::Execute(ConstructionalTransducer *t) {
  CreateStates(t);
  CreateArcs(t);
}

// =====================================================

void TiedModelTransducerInitialization::CreatePhoneModel(
    ModelManager *models, int phone, const PhoneContext &context) {
  if (phone_models_[phone])
    return;
  map<int, int>::const_iterator i = phone_mapping_.find(phone);
  if (i != phone_mapping_.end()) {
    VLOG(3) << "tied model: " << phone << " -> " << i->second;
    if (!phone_models_[i->second]) {
      BasicTransducerInitialization::CreatePhoneModel(
        models, i->second, context);
    }
    AllophoneModel *result = phone_models_[i->second];
    result->AddPhone(i->first);
    for (int s = 0; s < result->NumStates(); ++s) {
      PhoneContext *state_context = result->GetStateModel(s)->GetContextRef();
      state_context->GetContextRef(0)->Add(i->first);
    }
    phone_models_[i->first] = result;
  } else {
    VLOG(3) << "untied model: " << i->second;
    BasicTransducerInitialization::CreatePhoneModel(models, phone, context);
  }
}

void TiedModelTransducerInitialization::SetPhoneMap(
    const map<int, int> phone_map) {
  phone_mapping_ = phone_map;
}

// =====================================================

// Create a list of unique symbols, i.e. the set symbols that all phones are
// mapped to.
void SharedStateTransducerInitialization::CreateUnits(int num_phones) {
  for (int p = 0; p < num_phones; ++p) {
    map<int, int>::const_iterator m = phone_mapping_.find(p);
    if (m != phone_mapping_.end()) {
      units_.push_back(m->second);
      reverse_mapping_[m->second].push_back(m->first);
    } else {
      units_.push_back(p);
    }
  }
  RemoveDuplicates(&units_);
}

// Add all phones which are mapped to phone to the center
// context set of history.
void SharedStateTransducerInitialization::SetUnitHistory(
    int phone, PhoneContext *history) const {
  ContextSet &center = *history->GetContextRef(0);
  CHECK(center.IsEmpty());
  center.Add(phone);
  map<int, list<int> >::const_iterator i = reverse_mapping_.find(phone);
  if (i != reverse_mapping_.end()) {
    for (list<int>::const_iterator p = i->second.begin();
        p != i->second.end(); ++p)
      center.Add(*p);
  }
}

// Create the state mapping.
void SharedStateTransducerInitialization::CreateStates(
    ConstructionalTransducer *t) {
  CHECK(t->HasCenterSets());
  BasicTransducerInitialization::CreateStates(t);
  for (map<int, int>::const_iterator m = phone_mapping_.begin();
      m != phone_mapping_.end(); ++m) {
    CHECK_NOTNULL(phone_states_[m->second]);
    phone_states_[m->first] = phone_states_[m->second];
  }
}

// =====================================================

void WordBoundaryTransducerInitialization::FillSet(
    const vector<int> &src, set<int> *dst) const {
  std::copy(src.begin(), src.end(),
            std::insert_iterator< set<int> >(*dst, dst->begin()));
}

void WordBoundaryTransducerInitialization::SetInitialPhones(
    const vector<int> &initial_phones) {
  FillSet(initial_phones, &initial_phones_);
}

void WordBoundaryTransducerInitialization::SetFinalPhones(
    const vector<int> &final_phones) {
  FillSet(final_phones, &final_phones_);
}

void WordBoundaryTransducerInitialization::CreateArcs(
    ConstructionalTransducer *t) {
  int num_phones = phone_info_->NumPhones();
  for (vector<int>::const_iterator src_unit = units_.begin();
      src_unit != units_.end(); ++src_unit) {
    int src_phone = *src_unit;
    bool src_f = final_phones_.find(src_phone) != final_phones_.end();
    CHECK_NOTNULL(phone_states_[src_phone]);
    for (int next_phone = 0; next_phone < num_phones; ++next_phone) {
      bool dst_i = initial_phones_.find(next_phone) != initial_phones_.end();
      CHECK_NOTNULL(phone_states_[next_phone]);
      if (!(dst_i && !src_f)) {
        t->AddArc(phone_states_[src_phone], phone_states_[next_phone],
                  phone_models_[src_phone], next_phone);
      } else {
        VLOG(2) << "forbid arc: " << src_phone << " -> " << next_phone;
      }
    }
  }
}

// =====================================================

const char *TransducerInitializationFactory::kBasic = "basic";
const char *TransducerInitializationFactory::kTiedModel = "tiedmodel";
const char *TransducerInitializationFactory::kSharedState = "sharedstate";
const char *TransducerInitializationFactory::kWordBoundary = "wordboundary";

TransducerInitialization* TransducerInitializationFactory::Create(
    const string &name,
    const map<int, int> &phone_mapping,
    const vector<int> &initial_phones,
    const vector<int> &final_phones) {
  TransducerInitialization *result = NULL;
  TiedModelTransducerInitialization *tied = NULL;
  if (name == kBasic) {
    result = new BasicTransducerInitialization();
  } else if (name == kTiedModel) {
    result = tied = new TiedModelTransducerInitialization();
  } else if (name == kSharedState) {
    result = tied = new SharedStateTransducerInitialization();
  } else if (name == kWordBoundary) {
    CHECK(!initial_phones.empty());
    CHECK(!final_phones.empty());
    WordBoundaryTransducerInitialization *ti =
        new WordBoundaryTransducerInitialization();
    ti->SetInitialPhones(initial_phones);
    ti->SetFinalPhones(final_phones);
    result = tied = ti;
  }
  if (tied) {
    CHECK(!phone_mapping.empty());
    tied->SetPhoneMap(phone_mapping);
  }
  return result;
}

}  // namespace trainc
