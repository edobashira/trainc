// shifted_split_test.cc
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
// \file
// Test cases for splitting of shifted LexiconTransducer states

#include "lexicon_split_test.h"

namespace trainc {

class ShiftedLexiconSplitTest : public LexiconSplitTest
{
public:
  ShiftedLexiconSplitTest() {}

protected:
  virtual bool UseShifted() const { return true; }
};

TEST_F(ShiftedLexiconSplitTest, SplitLeft) {
  Init(5, 5);
  TestSplit(-1);
}

TEST_F(ShiftedLexiconSplitTest, SplitRight) {
  Init(5, 5);
  TestSplit(1);
}

TEST_F(ShiftedLexiconSplitTest, SplitLeftEpsilon) {
  Init(5, 5);
  TestSplitEpsilon(-1, 4, 0);
}

TEST_F(ShiftedLexiconSplitTest, SplitLeftEpsilonOnly) {
  Init(5, 5);
  TestSplitEpsilon(-1, 4, 1);
}

TEST_F(ShiftedLexiconSplitTest, SplitLeftEpsilonPath) {
  Init(5, 5);
  TestSplitEpsilon(-1, 5, 0);
}

TEST_F(ShiftedLexiconSplitTest, SplitRightEpsilon) {
  Init(5, 5);
  TestSplitEpsilon(1, 4, 0);
}

TEST_F(ShiftedLexiconSplitTest, SplitRightEpsilonOnly) {
  Init(5, 5);
  TestSplitEpsilon(1, 4, 1);
}

TEST_F(ShiftedLexiconSplitTest, SplitRightEpsilonPath) {
  Init(5, 5);
  TestSplitEpsilon(1, 5, 0);
}

TEST_F(ShiftedLexiconSplitTest, SplitBeforeLoop) {
  Init(5, 5);
  TestSplitLoop(1, false);
}

TEST_F(ShiftedLexiconSplitTest, SplitAfterLoop) {
  Init(5, 5);
  TestSplitLoop(-1, false);
}

TEST_F(ShiftedLexiconSplitTest, SplitLoopRight) {
  Init(5, 5);
  TestSplitLoop(1, true);}

TEST_F(ShiftedLexiconSplitTest, SplitLoopLeft) {
  Init(5, 5);
  TestSplitLoop(-1, true);
}

TEST_F(ShiftedLexiconSplitTest, Split3Phone) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      TearDown();
      SetUp();
      Init(nw, np);
      VerifyTransducer(false);
      SplitIndividual(100, 10, true);
    }
  }
}

TEST_F(ShiftedLexiconSplitTest, Split3PhoneShared) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      TearDown();
      SetUp();
      Init(nw, np, true);
      SplitIndividual(100, 10, true);
    }
  }
}


TEST_F(ShiftedLexiconSplitTest, Split3PhoneEpsilon) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      TearDown();
      SetUp();
      Init(nw, np, false, true);
      SplitIndividual(100, 10, true);
    }
  }
}

TEST_F(ShiftedLexiconSplitTest, Split3PhoneSharedEpsilon) {
  for (int np = 5; np < 10; ++np) {
    for (int nw = 2; nw <= 20; nw += 2) {
      TearDown();
      SetUp();
      Init(nw, np, true, true);
      VerifyTransducer();
      SplitIndividual(1, 10, true);
    }
  }
}



}  // namespace trainc
