// builder.cc
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
// Program to build the context dependency transducer.
// Read accumulated feature statistics and
// drives the construction of the context dependency transducer and the
// context dependent state models using trainc::ContextBuilder.
// The number of states for each HMM is deduced from the statistics read,
// missing HMM state statistics will therefore result in a wrong model.
//
// \file
// main executable for the context builder

#include <cstdlib>
#include <set>
#include <vector>
#include <fst/symbol-table.h>
#include "context_builder.h"
#include "hmm_compiler.h"
#include "sample.h"
#include "sample_reader.h"
#include "set_inventory.h"
#include "stringutil.h"
#include "util.h"

using std::set;
using std::vector;
using fst::SymbolTable;

// Input parameters:
// TODO(rybach): allow multiple sample data files.
DEFINE_string(samples_file, "", "sample data file");
// See SampleReader
DEFINE_string(sample_type, "text", "sample file type");
DEFINE_string(phone_syms, "", "labels for context (output) symbols");
DEFINE_int32(num_left_contexts, 1, "number of left context symbols");
DEFINE_int32(num_right_contexts, 1, "number of right context symbols");
DEFINE_string(boundary_context, "sil", "context label to use at boundaries");
DEFINE_string(phone_sets, "", "context class definitions");
DEFINE_double(min_split_gain, 0.0, "minimum gain for a node split");
DEFINE_int32(min_seen_contexts, 0, "minimum number of seen contexts per leaf");
DEFINE_int32(min_observations, 1000,
             "minimum number of observations per leaf");
DEFINE_double(variance_floor, 0.001,
               "minimum variance threshold for Gaussian models");
DEFINE_bool(use_composition, true,
            "use composition of C and the counting transducer");
DEFINE_bool(shifted_models, true,
            "shift models to arcs with the right context input label"
            "(requires --use_composition=true)");
DEFINE_bool(determistic_split, true,
            "spliting of (un-shifted) transducers produces input determistic"
            "arcs (requires --shifted_models=false)");
DEFINE_bool(ignore_absent_models, false,
             "do not consider models for splitting which are not part of the"
             "used counting transducer");
DEFINE_string(replay, "", "execute the splits from the given file");

// Output parameters:
DEFINE_string(ci_state_list, "", "list of context independent states");
DEFINE_string(hmm_list, "", "HMM list");
DEFINE_string(Ctrans, "", "C transducer output file");
DEFINE_string(CLtrans, "", "split counting transducer output file");
DEFINE_string(ci_hmmlist, "", "CI HMM list output file");
DEFINE_string(hmmlist, "", "HMM list output file");
DEFINE_string(hmm_to_phone_map, "", "CD HMM to phone map output file");
DEFINE_string(hmm_syms, "", "HMM symbol table output file");
DEFINE_string(leaf_model, "", "state distribution model output file");
// see ModelWriter
DEFINE_string(leaf_model_type, "", "type of state model output file");
DEFINE_string(state_syms, "", "States symbol table output file");
DEFINE_string(Htrans, "", "H transducer output file");

// Additional output parameters
DEFINE_string(cd2phone_hmm_name_map, "", "Name map from CD to phone HMMs");
DEFINE_string(cd2ci_state_name_map, "", "State name map from CD to CI states");
DEFINE_string(save_splits, "", "sequence of applied splits");

// Set number of HMM states per phone from file.
// If not set, the number of states is deduced from the statistics.
// Unobserved states may then lead to wrong phone length.
DEFINE_string(phone_length, "", "file containing the phone lengths");
// Control for the maximum number of HMM state models
DEFINE_int32(target_num_models, 0, "maximum number of HMM state models");
// Control for the size of the resulting transducer by limiting
// the number of its states
DEFINE_int32(target_num_states, 0, "maximum number of states");
// Control for the size of the resulting transducer by weighting the
// number of new states required by a phone model split.
DEFINE_double(state_penalty_weight, 10.0,
              "weight of the transducer size penalty");
// Define separate question sets per context position.
// Format for this flag is pos=file,pos=file,...
// with pos in [-num_left_contexts .. 0 .. num_right_contexts]
// positions which are not defined, are assigned to the default
// question set (flag phone_sets)
DEFINE_string(phone_sets_pos, "", "context class per position");
// Set the transducer is initialization method.
// Required for word boundary handling.
// See TransducerInitializationFactory.
DEFINE_string(transducer_init, "basic", "type of transducer initialization");
// Set a transducer used for counting required states,
// e.g. a closed, input epsilon free lexicon transducer
DEFINE_string(counting_transducer, "", "transducer used for counting states");
// Map central phones of allophone models to another phone. This is useful
// for adding wordboundary information to the phones in the context.
// Format: <from-phone-symbol> <to-phone-symbol>
// e.g A@i A
DEFINE_string(phone_map, "", "mapping of phones with tied models");
// Split center phone sets (context position 0).
// Required when states are shared between several
// phones (transducer_init=sharedstate).
DEFINE_bool(split_center_phone, false, "split sets of center phones");
// List of initial phones, one phone symbol per line
DEFINE_string(initial_phones, "", "file containing word initial phones");
// List of final phones, one phone symbol per line
DEFINE_string(final_phones, "", "file containing word end phones");
DEFINE_string(state_model_log, "", "state model information");
DEFINE_string(transducer_log, "", "transducer state information");
DEFINE_int32(max_hyps, 0, "maximum number of hypotheses evaluated");
DEFINE_int32(num_threads, 1, "number of threads used for split calculations");

namespace trainc {

class Builder {
 public:
  Builder() {}

  void main() {
    SetParameters();
    SymbolTable *phone_symbols = NULL;
    int num_phones;
    if (!LoadPhoneSymbols(FLAGS_phone_syms, &phone_symbols, &num_phones)) {
      REP(FATAL) << "cannot read context symbols";
      return;
    }
    set<int> ci_phones;
    if (!LoadCiStates(FLAGS_ci_state_list, *phone_symbols, &ci_phones)) {
      REP(FATAL) << "cannot read ci state list";
      return;
    }
    builder_.SetPhoneSymbols(*phone_symbols);
    builder_.SetCiPhones(ci_phones);
    builder_.SetBoundaryPhone(FLAGS_boundary_context);
    SetQuestionSets(*phone_symbols);

    if (!FLAGS_phone_map.empty())
      builder_.SetPhoneMapping(FLAGS_phone_map);
    if (!FLAGS_initial_phones.empty())
      builder_.SetInitialPhones(FLAGS_initial_phones);
    if (!FLAGS_final_phones.empty())
      builder_.SetFinalPhones(FLAGS_final_phones);

    SampleReader *reader = SampleReader::Create(FLAGS_sample_type);
    reader->SetPhoneSymbols(phone_symbols);
    Samples *samples = new Samples();
    samples->SetNumPhones(num_phones);
    reader->Read(FLAGS_samples_file, samples);
    builder_.SetSamples(samples);
    if (!FLAGS_phone_length.empty()) {
      builder_.SetPhoneLength(FLAGS_phone_length);
    } else {
      for (int phone = 0; phone < samples->NumPhones(); ++phone) {
        builder_.SetPhoneLength(phone, samples->NumStates(phone));
      }
    }
    delete phone_symbols;

    // generate the context dependency transducer.
    builder_.Build();
    WriteOutput();
  }

 private:
  // Forward flags to the ContextBuilder.
  void SetParameters() {
    builder_.SetReplay(FLAGS_replay);
    builder_.SetSaveSplits(FLAGS_save_splits);
    builder_.SetContextLength(FLAGS_num_left_contexts,
                              FLAGS_num_right_contexts,
                              FLAGS_split_center_phone);
    builder_.SetMinSplitGain(FLAGS_min_split_gain);
    builder_.SetMinSeenContexts(FLAGS_min_seen_contexts);
    builder_.SetMinObservations(FLAGS_min_observations);
    builder_.SetVarianceFloor(FLAGS_variance_floor);
    builder_.SetTargetNumModels(FLAGS_target_num_models);
    builder_.SetTargetNumStates(FLAGS_target_num_states);
    builder_.SetStatePenaltyWeight(FLAGS_state_penalty_weight);
    builder_.SetMaxHypotheses(FLAGS_max_hyps);
    builder_.SetTransducerInitType(FLAGS_transducer_init);
    builder_.SetCountingTransducer(FLAGS_counting_transducer);
    builder_.SetUseComposition(FLAGS_use_composition);
    builder_.SetShiftedTransducer(FLAGS_shifted_models);
    builder_.SetSplitDetermistic(FLAGS_determistic_split);
    builder_.SetIgnoreAbsentModels(FLAGS_ignore_absent_models);
  }

  void SetQuestionSets(const SymbolTable &phone_symbols) {
    SetInventory default_qs;
    if (!LoadQuestions(FLAGS_phone_sets, phone_symbols, &default_qs)) {
      REP(FATAL) << "cannot read question set " << FLAGS_phone_sets;
      return;
    }
    builder_.SetDefaultQuestionSet(default_qs);
    if (!FLAGS_phone_sets_pos.empty()) {
      vector<string> buffer, def;
      SplitStringUsing(FLAGS_phone_sets_pos, ",", &buffer);
      for (vector<string>::const_iterator s = buffer.begin();
          s != buffer.end(); ++s) {
        def.clear();
        SplitStringUsing(*s, "=", &def);
        CHECK_EQ(def.size(), 2);
        int pos = std::atoi(def[0].c_str());
        SetInventory qs;
        if (!LoadQuestions(def[1], phone_symbols, &qs))
          REP(FATAL) << "cannot read question set " << def[1];
        else
          builder_.SetQuestionSetPerContext(pos, qs);
      }
    }
  }

  // Load the phone symbol table.
  // The symbols are expected to be numbered continuously.
  bool LoadPhoneSymbols(const string &filename, SymbolTable **phone_symbols,
                        int *num_phones) {
    *phone_symbols = fst::SymbolTable::ReadText(filename);
    if (!*phone_symbols) {
      REP(ERROR) << "cannot read context symbols from " << filename;
      return false;
    }
    if ((*phone_symbols)->NumSymbols() != (*phone_symbols)->AvailableKey()) {
      REP(WARNING) << "expected continuous numbered symbols";
    }
    *num_phones = (*phone_symbols)->AvailableKey();
    return true;
  }

  // Load and parse the question sets.
  bool LoadQuestions(const string &filename, const SymbolTable &phone_symbols,
                     SetInventory *question_set) {
    question_set->SetSymTable(phone_symbols);
    question_set->ReadText(filename);
    return true;
  }

  bool ParseHmmStateSymbol(const SymbolTable &phone_symbols,
                           const string &symbol, int *phone, int *hmm_state) const {
    std::vector<string> tokens;
    SplitStringUsing(symbol, "_", &tokens);
    if (tokens.size() != 2) {
      REP(FATAL) << "parse error: " << symbol
          << " expected <phone_symbol>_<state>";
      return false;
    }
    *phone = phone_symbols.Find(tokens[0]);
    if (*phone < 0) {
      REP(FATAL) << "phone symbol " << tokens[0] << " not defined";
      return false;
    }
    *hmm_state = std::atoi(tokens[1].c_str());
    if (*hmm_state < 0) {
      REP(FATAL) << "invalid hmm state: " << tokens[1];
      return false;
    }
    return true;
  }

  // Parse list of context independent states and extract the CI phones.
  bool LoadCiStates(const string &filename, const SymbolTable &phone_symbols,
                    set<int> *ci_phones) {
    File *file = File::OpenOrDie(filename, "r");
    InputBuffer read_buffer(file);
    string line;
    int phone = 0, hmm_state = 0;
    while (read_buffer.ReadLine(&line)) {
      StripWhiteSpace(&line);
      if (line.empty()) continue;
      if (!ParseHmmStateSymbol(phone_symbols, line, &phone, &hmm_state)) {
        REP(ERROR) << "parse error in " << filename;
        read_buffer.CloseFile();
        return false;
      }
      ci_phones->insert(phone);
    }
    CHECK(read_buffer.CloseFile());
    return true;
  }

  // Generate and write all output files.
  void WriteOutput() {
    const HmmCompiler &hmm_compiler =
        builder_.GetHmmCompiler();
    if (!FLAGS_hmm_list.empty())
      hmm_compiler.WriteHmmList(FLAGS_hmm_list);
    if (!FLAGS_state_syms.empty())
      hmm_compiler.WriteStateSymbols(FLAGS_state_syms);
    if (!FLAGS_hmm_syms.empty())
      hmm_compiler.WriteHmmSymbols(FLAGS_hmm_syms);
    if (!FLAGS_leaf_model.empty()) {
      hmm_compiler.WriteStateModels(FLAGS_leaf_model, FLAGS_leaf_model_type,
          feature_type_, frontend_config_);
    }
    if (!FLAGS_cd2phone_hmm_name_map.empty())
      hmm_compiler.WriteCDHMMtoPhoneMap(FLAGS_cd2phone_hmm_name_map);
    if (!FLAGS_Htrans.empty())
      hmm_compiler.WriteHmmTransducer(FLAGS_Htrans);
    if (!FLAGS_cd2ci_state_name_map.empty())
      hmm_compiler.WriteStateNameMap(FLAGS_cd2ci_state_name_map);
    if (!FLAGS_Ctrans.empty())
      builder_.WriteTransducer(FLAGS_Ctrans);
    if (!FLAGS_CLtrans.empty())
      builder_.WriteCountingTransducer(FLAGS_CLtrans);
    if (!FLAGS_state_model_log.empty())
      hmm_compiler.WriteStateModelInfo(FLAGS_state_model_log);
    if (!FLAGS_transducer_log.empty())
      builder_.WriteStateInfo(FLAGS_transducer_log);
  }

  ContextBuilder builder_;
  string frontend_config_, feature_type_;

  DISALLOW_COPY_AND_ASSIGN(Builder);
};  // class Builder

}  // namespace trainc

int main(int argc, char **argv) {
  SetFlags("", &argc, &argv, true);
  using trainc::Builder;
  Builder builder;
  builder.main();
  return 0;
}

