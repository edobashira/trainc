// lexicon_transducer_test.cc
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
// Unit tests for LexiconTransducer

#include "unittest.h"
#include "lexicon_transducer.h"

namespace trainc {

class LexiconTransducerTest : public ::testing::Test
{
public:
  LexiconTransducerTest() : l_(NULL), m_(NULL) {}

  void SetUp() {
    l_ = new LexiconTransducer();
    l_->SetContextSize(kNumPhones, 1, 1, false);
    m_ = new AllophoneModel(1, 0);
  }

  void TearDown() {
    delete l_;
    l_ = NULL;
    delete m_;
    m_ = NULL;
  }

protected:
  typedef LexiconTransducer::State State;
  typedef LexiconTransducer::StateId StateId;
  typedef LexiconTransducer::Arc Arc;
  static const int kNumPhones = 5;
  LexiconTransducer *l_;
  const AllophoneModel *m_;
};

TEST_F(LexiconTransducerTest, RemoveState) {
  StateId s1 = l_->AddState();
  l_->AddState();
  l_->RemoveState(s1);
  l_->PurgeStates();
  StateId s3 = l_->AddState();
  EXPECT_EQ(s1, s3);
}

TEST_F(LexiconTransducerTest, AddArc) {
  StateId s = l_->AddState();
  StateId n = l_->AddState();
  State::ArcRef a = l_->AddArc(s, Arc(1, 2, m_, fst::StdArc::Weight::One(), n));
  EXPECT_EQ(a->prevstate, s);
  EXPECT_EQ(a->nextstate, n);
  EXPECT_EQ(a->model, m_);
  EXPECT_EQ(l_->NumArcs(s), size_t(1));
  EXPECT_EQ(l_->NumArcs(n), size_t(0));
}

TEST_F(LexiconTransducerTest, IncomingArcs) {
  StateId s = l_->AddState();
  StateId n = l_->AddState();
  State::ArcRef a = l_->AddArc(s, Arc(1, 2, m_, fst::StdArc::Weight::One(), n));
  const State::ArcRefSet &incoming = l_->GetState(n)->GetIncomingArcs();
  EXPECT_EQ(incoming.size(), size_t(1));
  EXPECT_EQ((*incoming.begin())->prevstate, s);
  EXPECT_EQ((*incoming.begin())->model, m_);
}

TEST_F(LexiconTransducerTest, LexiconArcIterator) {
  StateId s = l_->AddState();
  const int nArcs = 10;
  for (int i = 0; i < nArcs; i++) {
    l_->AddArc(s, Arc(i, i, m_, fst::StdArc::Weight::One(), l_->AddState()));
  }
  EXPECT_EQ(l_->NumArcs(s), size_t(nArcs));
  for (LexiconArcIterator aiter(*l_, s); !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    EXPECT_EQ(arc.ilabel, arc.olabel);
  }
}

TEST_F(LexiconTransducerTest, ArcIterator) {
  StateId s = l_->AddState();
  const int nArcs = 10;
  for (int i = 0; i < nArcs; i++) {
    l_->AddArc(s, Arc(i, i, m_, fst::StdArc::Weight::One(), l_->AddState()));
  }
  EXPECT_EQ(l_->NumArcs(s), size_t(nArcs));
  for (fst::ArcIterator<LexiconTransducer> aiter(*l_, s); !aiter.Done();
      aiter.Next()) {
    const Arc &arc = aiter.Value();
    EXPECT_EQ(arc.ilabel, arc.olabel);
  }
}

TEST_F(LexiconTransducerTest, LexicionArcIteratorRemoved) {
  StateId s = l_->AddState();
  const int nArcs = 10;
  for (int i = 0; i < nArcs; i++) {
    l_->AddArc(s, Arc(i, i, m_, fst::StdArc::Weight::One(), l_->AddState()));
  }
  l_->RemoveState(s);
  EXPECT_EQ(l_->NumArcs(s), size_t(0));
  LexiconArcIterator aiter(*l_, s);
  EXPECT_TRUE(aiter.Done());
}

TEST_F(LexiconTransducerTest, ArcIteratorRemoved) {
  StateId s = l_->AddState();
  const int nArcs = 10;
  for (int i = 0; i < nArcs; i++) {
    l_->AddArc(s, Arc(i, i, m_, fst::StdArc::Weight::One(), l_->AddState()));
  }
  l_->RemoveState(s);
  EXPECT_EQ(l_->NumArcs(s), size_t(0));
  fst::ArcIterator<LexiconTransducer> aiter(*l_, s);
  EXPECT_TRUE(aiter.Done());
}

TEST_F(LexiconTransducerTest, RemoveArc) {
  StateId s = l_->AddState();
  const int nArcs = 10;
  State::ArcRef del;
  StateId ds = fst::kNoStateId;
  const int deleteLabel = 4;
  for (int i = 0; i < nArcs; i++) {
    StateId n = l_->AddState();
    State::ArcRef a = l_->AddArc(s,
        Arc(i, i, m_, fst::StdArc::Weight::One(), n));
    if (i == deleteLabel) {
      del = a;
      ds = n;
    }
  }
  l_->RemoveArc(s, del);
  EXPECT_EQ(l_->NumArcs(s), size_t(nArcs - 1));
  EXPECT_EQ(l_->GetState(ds)->GetIncomingArcs().size(), size_t(0));
  for (LexiconArcIterator aiter(*l_, s); !aiter.Done(); aiter.Next()) {
    const Arc &arc = aiter.Value();
    EXPECT_NE(arc.ilabel, deleteLabel);
  }
}

}  // namespace trainc
