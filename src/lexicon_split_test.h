// lexicon_split_test.h
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
// Copyright 2012 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file test cases for splitting of LexiconTransducer states

#ifndef LEXICON_SPLIT_TEST_H_
#define LEXICON_SPLIT_TEST_H_

#include <vector>
#include "fst/symbol-table.h"
#include "fst/vector-fst.h"
#include "transducer_test.h"
#include "lexicon_transducer.h"

namespace trainc {

class LexiconSplitTest : public ConstructionalTransducerTest
{
public:
  LexiconSplitTest()
      : l_(NULL), lexicon_(NULL), phone_symbols_(NULL) {}
  void SetUp() {
    phone_symbols_ = new fst::SymbolTable("phones");
  }
  void TearDown() {
    ConstructionalTransducerTest::TearDown();
    delete phone_symbols_;
    delete l_;
    delete lexicon_;
  }
protected:
  typedef LexiconTransducer::StateId StateId;
  typedef fst::StdArc::Label Label;
  typedef fst::StdArc::Weight Weight;

  virtual StateCountingTransducer* GetC() { return l_; }
  void Init(int num_words, int num_phones, bool center_set = false,
            bool insert_eps = false, bool deterministic = true);
  virtual void VerifyTransducer() {
    VerifyTransducer(true);
  }
  void VerifyTransducer(bool checkWords);
  void VerifyComposition() const;
  void CreateLexicon(bool insert_eps);
  void CreatePhoneOutput(const LexiconTransducer &l,
                         fst::StdVectorFst *t) const;
  void CreateWord(const vector<Label> &word, fst::StdVectorFst *w) const;
  ModelManager::StateModelRef GetStateModel(int phone) const;
  void Split(ModelManager::StateModelRef state_model, int context_pos,
             const ContextQuestion &q);

  void TestSplit(int context_pos);
  void TestSplitEpsilon(int context_pos, int nstates, int n_eps_only);
  void TestSplitLoop(int context_pos, bool split_loop);
  void AppendSilence(fst::StdVectorFst *f) const;
  virtual bool UseShifted() const { return false; }
  static const int kNumLeftContext;
  static const int kNumRightContext;
  static const int kSilPhone;
  std::vector< pair<ContextSet, ContextSet> > state_context_;
  std::vector< vector<Label> > words_;
  LexiconTransducer *l_;
  fst::StdVectorFst *lexicon_;
  fst::SymbolTable *phone_symbols_;
  int num_words_, num_phones_;
};

}  // namespace trainc

#endif  // LEXICON_SPLIT_TEST_H_
