// stringutil_test.cc
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
// Copyright 2010 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Test of functions in stringutil.h

#include "stringutil.h"
#include "unittest.h"

using std::string;

namespace trainc {

TEST(StringUtil, StripWhiteSpace) {
  string a = "  abc";
  StripWhiteSpace(&a);
  EXPECT_EQ(a, string("abc"));

  string b = "abc   ";
  StripWhiteSpace(&b);
  EXPECT_EQ(b, string("abc"));

  string c = "   abc   ";
  StripWhiteSpace(&c);
  EXPECT_EQ(c, string("abc"));

  string d = " \t  abc  \n";
  StripWhiteSpace(&d);
  EXPECT_EQ(d, string("abc"));
}

TEST(StringUtil, SplitStringUsing) {
  std::vector<string> v;
  string s = "abc def ghi jklmn";
  SplitStringUsing(s, " ", &v);
  EXPECT_EQ(v.size(), size_t(4));
  EXPECT_EQ(v[0], string("abc"));
  EXPECT_EQ(v[1], string("def"));
  EXPECT_EQ(v[2], string("ghi"));
  EXPECT_EQ(v[3], string("jklmn"));
}

TEST(StringUtil, JoinStringsUsing) {
  std::vector<string> v;
  string s = "abc def ghi jklmn";
  SplitStringUsing(s, " ", &v);
  EXPECT_EQ(v.size(), size_t(4));
  string r;
  JoinStringsUsing(v, " ", &r);
  EXPECT_EQ(s, r);
}

TEST(StringUtil, StringPrintf) {
  string a = StringPrintf("%d", 1);
  EXPECT_EQ(a, string("1"));
  string b = StringPrintf("%0.2f", .23);
  EXPECT_EQ(b, string("0.23"));
  string c = StringPrintf("%d %.4f", 10, 1.2345);
  EXPECT_EQ(c, string("10 1.2345"));
}

}  // namespace trainc
