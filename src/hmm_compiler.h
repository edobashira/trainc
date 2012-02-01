// hmm_compiler.h
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
// Definition of HmmCompiler, which creates and writes all
// output data related to the construction of context dependent models.

#ifndef HMM_COMPILER_H_
#define HMM_COMPILER_H_

#include <string>
#include <vector>
#include <ext/hash_map>
#include "fst/fst-decl.h"
using __gnu_cxx::hash_map;
using std::string;
using std::vector;
#include "util.h"


namespace fst {
class SymbolTable;
}

namespace trainc {

class ModelManager;
class Phones;
class AllophoneStateModel;
class AllophoneModel;
class GaussianModel;

// Creates and writes the following data:
//  * hmm list of context dependent HMMS
//  * leaf distribution of CD state models
//  * symbol tables
//  * CD to CI mapping (HMM and HMM state)
//  * HMM transducer
//
// After setting the data members, EnumerateModels() has to be called to
// construct the HMMs and HMM state models. Afterwards the Write* methods
// can be called in arbitrary order.
class HmmCompiler {
 public:
  HmmCompiler();
  ~HmmCompiler();

  // Set the models constructed by the ContextBuilder.
  void SetModels(const ModelManager *models) {
    models_ = models;
  }

  // Set the phone informations
  void SetPhoneInfo(const Phones *phone_info) {
    phone_info_ = phone_info;
  }

  // Set the phone symbol table
  void SetPhoneSymbols(const fst::SymbolTable *phone_symbols) {
    phone_symbols_ = phone_symbols;
  }

  // Set the minimum variance for the estimated state models
  void SetVarianceFloor(float variance_floor) {
    variance_floor_ = variance_floor;
  }

  // Build the list of state models and HMM models.
  // This method has to be called before any of the
  // writing methods can be called.
  void EnumerateModels();

  // Write the hmm list.
  void WriteHmmList(const string &filename) const;

  // Write the models of the HMM states (aka leaf distributions).
  // Set the feature_type and frontend_config used to estimate the
  // state models.
  void WriteStateModels(const string &filename, const string &file_type,
                        const string &feature_type,
                        const string &frontend_config) const;

  // Write the symbol table for the HMM state models.
  void WriteStateSymbols(const string &filename) const;

  // Write the symbol table for the HMM models
  void WriteHmmSymbols(const string &filename) const;

  // Write a mapping from HMM names to phone names.
  void WriteCDHMMtoPhoneMap(const string &filename) const;

  // Write a mapping from CD to CI HMM state names.
  void WriteStateNameMap(const string &filename) const;

  // Write the H transducer (from context dependent HMM state symbols to
  // context dependent HMM symbols).
  void WriteHmmTransducer(const string &filename) const;

  // Write information about the state models
  void WriteStateModelInfo(const string &filename) const;

  // Get the symbol name for the given phone model.
  // The name corresponds to an entry in the HMM symbol table.
  string GetHmmName(const AllophoneModel *phone_model) const;

  // Return the symbol table used to index the context dependent HMMs.
  const fst::SymbolTable& GetHmmSymbols() const;

  // Number of generated HMM state models.
  int NumStateModels() const;

  // Number of generated HMMs
  int NumHmmModels() const;

  // Return the generated HMM state model.
  // This method is very inefficient, because a requires a linear scan through
  // all generated state models.
  // Intended for testing purposes only.
  // phone is the index in the phone symbol table.
  // hmm_state has to be in [0 .. #states)
  vector<const AllophoneStateModel*> GetStateModels(int phone,
                                                    int hmm_state) const;

  // Create a non-deterministic C transducer (straight-forward construction).
  void WriteNonDetC(const std::string &filename, int boundary_phone) const;
 private:
  typedef hash_map<const AllophoneStateModel*, string,
                   PointerHash<const AllophoneStateModel> > StateModelMap;
  typedef hash_map<const AllophoneModel*, int,
                   PointerHash<const AllophoneModel> > PhoneModelMap;
  void InitStateModelIndex();
  void InitSymbols();
  void AddPhoneModel(const AllophoneModel *phone_model);
  void AddStateModel(const AllophoneStateModel *state_model);
  void CreateStateModels();
  string GetHmmStateName(const AllophoneStateModel *state_model) const;

  const ModelManager *models_;
  const Phones *phone_info_;
  const fst::SymbolTable *phone_symbols_;
  fst::SymbolTable *hmm_state_symbols_, *hmm_symbols_;
  StateModelMap state_models_;
  PhoneModelMap phone_models_;
  GaussianModel *model_;
  float variance_floor_;
  vector< vector<int> > state_model_index_;
  int next_hmm_index_;

  // name used for the HMM state symbol table.
  static const char *kHmmStateSymbolsName;
  // name used for the HMM symbol table.
  static const char *kHmmSymbolsName;
};

}  // namespace trainc

#endif  // HMM_COMPILER_H_
