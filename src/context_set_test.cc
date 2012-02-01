// context_set_test.cc
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
// Tests for PhoneContext and ContextQuestion.

#include "unittest.h"
#include "context_set.h"

namespace trainc {

TEST(PhoneContextTest, Test) {
  const int num_phones = 10;
  const int nl = 3, nr = 4;
  PhoneContext p(num_phones, nl, nr);
  EXPECT_EQ(p.NumLeftContexts(), nl);
  EXPECT_EQ(p.NumRightContexts(), nr);

  ContextSet a(num_phones), b(num_phones);
  a.Add(1);
  b.Add(2);
  p.SetContext(1, a);
  EXPECT_TRUE(p.GetContext(1).IsEqual(a));
  p.SetContext(-1, b);
  EXPECT_TRUE(p.GetContext(-1).IsEqual(b));
  for (int i = -nl; i <= nr; ++i) {
    if (i == 0 || abs(i) == 1) continue;
    EXPECT_TRUE(p.GetContext(i).IsEmpty());
  }
  EXPECT_NE(p.HashValue(), 0);

  PhoneContext pa(num_phones, nl, nr), pb(num_phones, nl, nr);
  pa.SetContext(1, a);
  pa.SetContext(-1, b);
  pb.SetContext(1, a);
  pb.SetContext(-2, b);
  EXPECT_TRUE(p.IsEqual(p));
  EXPECT_TRUE(p.IsEqual(pa));
  EXPECT_FALSE(p.IsEqual(pb));
}

TEST(ContextQuestionTest, HasElement) {
  const int num_phones = 20;
  ContextSet c(num_phones);
  c.Add(4);
  c.Add(7);
  c.Add(19);
  ContextQuestion question(c);
  for (int i = 0; i < num_phones; ++i) {
    EXPECT_NE(question.GetPhoneSet(0).HasElement(i),
              question.GetPhoneSet(1).HasElement(i));
  }
}

}  // namespace trainc
