// composed_transducer_test.cc
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
// Tests for CountingTransducerBuilder

#include "fst/compose.h"
#include "fst/determinize.h"
#include "fst/minimize.h"
#include "fst/project.h"
#include "fst/vector-fst.h"
#include "fst/symbol-table.h"
#include "composed_transducer.h"
#include "transducer.h"
#include "transducer_compiler.h"
#include "transducer_init.h"
#include "transducer_test.h"
#include "phone_models.h"
#include "stringutil.h"
#include "unittest.h"


namespace trainc {

class ComposedTransducerTest : public ConstructionalTransducerTest {
public:
  ComposedTransducerTest() :
    phone_syms_(NULL), l_(NULL), cl_(NULL) {}
  virtual void SetUp() {
    phone_syms_ = new fst::SymbolTable("phones");
  }
  virtual void TearDown() {
    ConstructionalTransducerTest::TearDown();
    delete cl_;
    delete l_;
    delete phone_syms_;
  }
protected:
  void Init(int num_phones, int num_left_context, int num_words,
            bool center_set);
  void TestBuild();
  void CreateLexicon();
  void CreateComposed();
  virtual StateCountingTransducer* GetC() {
    return cl_;
  }
  fst::SymbolTable *phone_syms_;
  fst::StdVectorFst *l_;
  ComposedTransducer *cl_;

  int num_words_;
  static const int kNumRightContext = 1;
  static const int kSilPhone = 0;
};


void ComposedTransducerTest::Init(int num_phones, int num_left_context,
                                  int num_words, bool center_set) {
  ConstructionalTransducerTest::Init(num_phones, num_left_context,
                                     kNumRightContext, center_set);
  num_words_ = num_words;
  for (int p = 0; p < num_phones_; ++p) {
    if (p == kSilPhone) {
      phone_syms_->AddSymbol("si", p + 1);
    } else {
      phone_syms_->AddSymbol(StringPrintf("%c", 'a' + p - 1), p + 1);
    }
  }
  if (center_set_)
    InitSharedStateTransducer();
  else
    InitTransducer();
  CHECK(c_);
  CreateLexicon();
  CreateComposed();
}

void ComposedTransducerTest::CreateLexicon() {
  typedef fst::StdArc::StateId StateId;
  typedef fst::StdArc::Weight Weight;
  typedef fst::StdArc::Label Label;
  std::vector< std::vector<Label> > words(num_words_);
  for (int w = 0; w < num_words_; ++w) {
    int len = (w + 2) / 2;
    words[w].reserve(len);
    Label c = w + 1;
    for (int l = 0; l < len; ++l, ++c)
      words[w].push_back((c % (num_phones_ - 1)) + 2);
  }
  fst::StdVectorFst lexicon;
  StateId root = lexicon.AddState();
  lexicon.SetStart(root);
  lexicon.SetFinal(root, Weight::One());
  lexicon.SetInputSymbols(NULL);
  lexicon.AddArc(root, fst::StdArc(kSilPhone + 1, 0, Weight::One(), root));
  for (int w = 0; w < num_words_; ++w) {
    StateId s = root;
    for (int l = 0; l < words[w].size(); ++l) {
      StateId n = lexicon.AddState();
      lexicon.AddArc(s, fst::StdArc(words[w][l], 0, Weight::One(), n));
      s = n;
    }
    lexicon.AddArc(s, fst::StdArc(0, 0, Weight::One(), root));
  }
  l_ = new fst::StdVectorFst;
  fst::Determinize(lexicon, l_);
  fst::Minimize(l_);
  fst::ArcSort(l_, fst::StdILabelCompare());
}

void ComposedTransducerTest::CreateComposed() {
  cl_ = new ComposedTransducer();
  cl_->SetBoundaryPhone(kSilPhone);
  cl_->SetCTransducer(c_);
  cl_->SetLTransducer(*l_);
  cl_->Init();
}

void ComposedTransducerTest::TestBuild() {
}


TEST_F(ComposedTransducerTest, Build1) {
  Init(4, 1, 4, false);
  TestBuild();
}

TEST_F(ComposedTransducerTest, Build2) {
  Init(5, 2, 10, false);
  TestBuild();
}

TEST_F(ComposedTransducerTest, Build3) {
  Init(5, 3, 10, false);
  TestBuild();
}

TEST_F(ComposedTransducerTest, BuildWb) {
  Init(5, 3, 10, true);
  TestBuild();
}


TEST_F(ComposedTransducerTest, BuildLarge) {
  Init(40, 3, 1000, false);
  TestBuild();
}

TEST_F(ComposedTransducerTest, SplitPrediction3Phone) {
  for (int np = 2; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      VLOG(1) << "";
      VLOG(1) << "np=" << np << " nw=" << nw;
      TearDown();
      SetUp();
      Init(np, 1, nw, false);
      SplitIndividual(100, 10, true);
    }
  }
}

TEST_F(ComposedTransducerTest, SplitPrediction3PhoneShared) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      VLOG(1) << "";
      VLOG(1) << "np=" << np << " nw=" << nw;
      TearDown();
      SetUp();
      Init(np, 1, nw, true);
      SplitIndividual(100, 10, true);
    }
  }
}

TEST_F(ComposedTransducerTest, SplitPrediction4Phone) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      VLOG(1) << "";
      VLOG(1) << "np=" << np << " nw=" << nw;
      TearDown();
      SetUp();
      Init(np, 2, nw, false);
      SplitIndividual(100, 10, true);
    }
  }
}

TEST_F(ComposedTransducerTest, SplitPrediction4PhoneShared) {
  Init(9, 2, 2, true);
  SplitIndividual(100, 10, true);

  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      VLOG(1) << "";
      VLOG(1) << "np=" << np << " nw=" << nw;
      TearDown();
      SetUp();
      Init(np, 2, nw, true);
      SplitIndividual(100, 10, true);
    }
  }
}


TEST_F(ComposedTransducerTest, SplitPrediction5Phone) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      VLOG(1) << "";
      VLOG(1) << "np=" << np << " nw=" << nw;
      TearDown();
      SetUp();
      Init(np, 3, nw, false);
      SplitIndividual(100, 10, true);
    }
  }
}

}  // namespace trainc
