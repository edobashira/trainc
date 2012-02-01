// context_builder.h
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
// Context dependency transducer construction from data without
// explicit construction of decision trees. This builds the context
// dependency transducer and the HMM state models (aka leaf models).
// In contrast to the decision tree growing (see speech/trainer/mr_tree),
// it is possible to control for the size of the resulting transducer,
// see ContextBuilder::SetStatePenaltyWeight().
// Currently, the number or right contexts is limited to 1, whereas the
// number of left contexts is not limited.
// The topology of the HMMs is limited to left-to-right HMMs, without sharing
// of HMM states between HMMs of different phones.
// Input: Pre-computed statistics (in Samples)

#ifndef CONTEXT_BUILDER_H_
#define CONTEXT_BUILDER_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "fst/fst-decl.h"
#include "phone_models.h"
#include "util.h"

using std::list;
using std::map;
using std::set;
using std::string;
using std::vector;

namespace trainc {

class ComposedTransducer;
class ConstructionalTransducer;
class HmmCompiler;
class LexiconTransducer;
class ModelSplitter;
class Samples;
class SetInventory;
class MaximumLikelihoodScorer;


// Constructs the context dependency transducer and tied contexted dependent
// HMM state models.
// The construction is data driven without explicitly growing
// phonetic decision trees.
// The HMM state model to split, the context position, and the question used
// to partition the context phones is chosen with a greedy optimization
// strategy, one model split per iteration.
// The objective function used accounts for both the gain in acoustic
// likelihood, achieved by splitting the HMM state model, and the number of
// states in the context dependency transducer, required to distinguish
// between the contexts of the split HMM models.
class ContextBuilder {
 public:
  typedef vector<ContextQuestion*> QuestionSet;

  ContextBuilder();
  ~ContextBuilder();

  // if !filename.empty(), instead of optimizing splits, the splits
  // stored in the given file are executed.
  // This method has to be called before any other method.
  void SetReplay(const std::string &filename);

  // Save the sequence of splits performed in the given file.
  void SetSaveSplits(const std::string &filename);

  // Set the used phone symbols.
  void SetPhoneSymbols(const fst::SymbolTable &phone_symbols);

  // Define the context independent phones
  void SetCiPhones(const set<int> &ci_phones);

  // Reference to the symbol table of phones used.
  const fst::SymbolTable& GetPhoneSymbols() const { return *phone_symbols_; }

  // Parameters

  // Set the number of phones to the left and to the right that
  // are used as context for all context dependent phones.
  // If split_center = true, allow splits on the center phone
  // (requires mapped states).
  void SetContextLength(int left_context, int right_context, bool split_center);

  // Set the minimum gain that must be achieved by a state model split
  // to be considered during the construction of the transducer.
  // The transducer construction terminates when the maximum split gain
  // falls below this value.
  void SetMinSplitGain(float min_gain);

  // Set the minimum number of seen contexts for a state model.
  // If a prospective split produces a state model with fewer than
  // min_contexts observed contexts, the split is ignored and not
  // evaluated.
  void SetMinSeenContexts(int num_contexts);

  // Set the minimum number of observations for a state model.
  // If a prospective split produces a state model with fewer than
  // num_obs observations, the split is ignored and not evaluated.
  void SetMinObservations(int num_obs);

  // Set the variance floor. If the variance of a leaf distribution
  // falls below floor, the variance is set to variance_floor for the
  // purpose of cost evaluation.
  void SetVarianceFloor(float floor);

  // Set maximum number of state models to build. If set to zero the
  // number of state models is not limited.
  void SetTargetNumModels(int num_models);

  // Set maximum number of states in the transducer. If set to zero
  // number of states is not limited.
  void SetTargetNumStates(int num_states);

  // Set maximum number of hypotheses evaluated. Hypotheses are processed
  // ordered by their achived gain.
  void SetMaxHypotheses(int max_hyps);

  // Set the weight of the state penalty. This scaling factor is applied to
  // the number of new states in the context dependency transducer required
  // by a prospective model split during optimization of model splits.
  void SetStatePenaltyWeight(float weight);

  // Set the name of the phone used for boundary context.
  // Requires that SetPhoneSymbols has been called before.
  void SetBoundaryPhone(const string &phone_name);

  // Set the phonetic questions used to split a set of context phones.
  // Sets the default questions set which is used for all context positions
  // that are not assigned a designated question set
  // using SetQuestionSetPerContext.
  // SetPhoneSymbols() must have been called before.
  // SetContextLength() must have been called before.
  void SetDefaultQuestionSet(const SetInventory &questions_set);

  // Set a separate questions for a specific context position.
  // context_position is [-num_left_context .. 0 .. num_right_context]
  // Must be called after SetQuestionSet.
  void SetQuestionSetPerContext(int context_position,
                                const SetInventory &questions);

  // Define the number of HMM states for a phone.
  // Has to be called for each HMM state of each phone used.
  void SetPhoneLength(int phone, int num_states);

  // Define the number of HMM states for all phones.
  // Read from file.
  // File format: <phone> <num-states>
  void SetPhoneLength(const std::string &filename);

  // Set a phone to phone mapping for phones that share their models.
  // This is useful for incorporating word boundary information.
  // File format: <phone-symbol-from> <phone-symbol-to>
  void SetPhoneMapping(const string &filename);

  // Set the transducer initialization method.
  // Possible values are: "wordboundary", "tiedmodel", "basic" == ""
  void SetTransducerInitType(const string &init_type);

  // Set the transducer used for counting required states.
  // Optional, by default the context dependency transducer itself
  // is used for counting states.
  void SetCountingTransducer(const string &filename);

  // Use the composition of C and a transducer (set by SetCountingTranducer)
  // for state counting. If set to false, the counting transducer itself is
  // split directly. Requires SetCountingTransducer()
  void SetUseComposition(bool use_composition);

  // If shifted == true, the counting transducer uses shifted models
  // (i.e. models appear at the arc with the the right context input label.
  // Requires SetUseComposition(false)
  void SetShiftedTransducer(bool shifted);

  // If split_deterministic == true, the counting transducer splitting
  // generates (input) deterministic arcs.
  // Requires SetShiftedTransducer(false)
  void SetSplitDetermistic(bool split_determistic);

  // Set whether models not present in the transducer should be considered
  // for splitting.
  void SetIgnoreAbsentModels(bool ignore);

  // Set initial phones, i.e. phones occurring at word begin.
  void SetInitialPhones(const vector<string> &initial_phones);

  // Read initial phones from file.
  void SetInitialPhones(const string &filename);

  // Set final phones, i.e. phones occurring at word ends.
  void SetFinalPhones(const vector<string> &final_phones);

  // Read final phones from file.
  void SetFinalPhones(const string &filename);

  // Set the training samples.
  // Takes ownership of the Samples object.
  void SetSamples(const Samples *samples);

  // Optimize the set of context dependent state models and build the
  // context dependency transducer.
  void Build();

  // Check the intermediate transducer for valid structure.
  // Mainly intended for unit tests
  bool CheckTransducer() const;

  // Return a reference to the HmmCompiler used to create the final
  // HMMs and state models.
  const HmmCompiler& GetHmmCompiler() const;

  // Construct and write the final context dependency transducer.
  void WriteTransducer(const string &filename) const;

  // Construct and write the split counting transducer.
  // Requires SetCountingTransducer(...) and SetUseComposition(false)
  void WriteCountingTransducer(const string &filename) const;

  // Write a file containing state descriptions
  void WriteStateInfo(const string &filename) const;

  // Number of states in the transducer
  int NumStates() const;

 private:
  void ConvertPhones(const vector<string> &src, vector<int> *dst) const;
  void ConvertPhonesFromFile(const string &filename, vector<int> *dst) const;
  void ConvertQuestionSet(const SetInventory &set_inventory,
                          QuestionSet *question_set) const;
  ComposedTransducer* CreateComposedTransducer(const string &l_file,
                                               ConstructionalTransducer *c) const;
  LexiconTransducer* CreateLexiconTransducer(const std::string &l_file,
                                            ConstructionalTransducer *c) const;
  ConstructionalTransducer* CreateTransducer(ModelManager *models) const;

  const fst::SymbolTable *phone_symbols_;
  set<int> ci_phones_;
  int num_phones_;
  Phones *phone_info_;
  ContextSet *all_phones_;
  map<int, int> phone_mapping_;
  vector<int> initial_phones_, final_phones_;
  string transducer_init_;
  string counting_transducer_file_;
  bool use_composition_;
  ConstructionalTransducer *transducer_;
  LexiconTransducer *cl_transducer_;
  HmmCompiler *hmm_compiler_;
  ModelManager *models_;
  MaximumLikelihoodScorer *scorer_;
  int boundary_phone_;
  int num_left_contexts_, num_right_contexts_;
  bool split_center_, shifted_cl_, determistic_cl_;
  float variance_floor_;
  list<QuestionSet> question_sets_;
  ModelSplitter *builder_;
  DISALLOW_COPY_AND_ASSIGN(ContextBuilder);
};

}  // namespace trainc

#endif  // CONTEXT_BUILDER_H_
