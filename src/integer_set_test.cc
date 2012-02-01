// integer_set_test.cc
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
// Tests for IntegerSet.

#include "unittest.h"
#include "util.h"
#include "integer_set.h"

using std::vector;

namespace trainc {

// Tests to validate that IntegerSet has set properties.
template<size_t max_elements>
class IntegerSetTest {
 public:
  IntegerSetTest()
      : a_(NULL), b_(NULL), ab_(NULL),
        empty_(NULL), all_(NULL) {}

  void TearDown() {
    delete a_;
    delete b_;
    delete ab_;
    delete empty_;
    delete all_;
  }

  typedef void (IntegerSetTest::*TestFunction)();
  void TestAllSizes(TestFunction function) {
    for (size_t v = 4; v <= max_elements; ++v) {
      Init(v);
      (this->*function)();
      TearDown();
    }
  }

  void TestSize() {
    EXPECT_EQ(empty_->Size(), size_t(0));
    EXPECT_EQ(all_->Size(), size_t(num_elements));
    EXPECT_EQ(a_->Size(), common_values.size() + 1);
    EXPECT_EQ(b_->Size(), common_values.size() + 1);
    EXPECT_EQ(ab_->Size(), common_values.size());
  }

  void TestHasMember() {
    size_t n = common_values.size();
    for (int i = 0; i < n; ++i) {
      EXPECT_TRUE(a_->HasElement(common_values[i]));
      EXPECT_TRUE(b_->HasElement(common_values[i]));
    }
    for (int p = 0; p < num_elements; ++p) {
      EXPECT_TRUE(all_->HasElement(p));
    }
    for (int p = 0; p < num_elements; ++p) {
      EXPECT_FALSE(empty_->HasElement(p));
    }
  }

  void TestIsEmpty() {
    EXPECT_TRUE(empty_->IsEmpty());
    EXPECT_FALSE(a_->IsEmpty());
  }

  void TestIsSubset() {
    EXPECT_TRUE(a_->IsSubSet(*all_));
    EXPECT_TRUE(b_->IsSubSet(*all_));
    EXPECT_FALSE(a_->IsSubSet(*b_));
    EXPECT_FALSE(b_->IsSubSet(*a_));
    EXPECT_FALSE(all_->IsSubSet(*b_));
    EXPECT_FALSE(all_->IsSubSet(*a_));
    EXPECT_TRUE(empty_->IsSubSet(*a_));
  }

  void TestIsEqual() {
    EXPECT_TRUE(a_->IsEqual(*a_));
    EXPECT_FALSE(a_->IsEqual(*b_));
    EXPECT_FALSE(a_->IsEqual(*empty_));
    EXPECT_FALSE(a_->IsEqual(*all_));
  }

  void TestIntersect() {
    IntSet ia = *a_;
    ia.Intersect(*all_);
    EXPECT_TRUE(a_->IsEqual(ia));
    IntSet iab = *a_;
    iab.Intersect(*b_);
    EXPECT_TRUE(iab.IsEqual(*ab_));
  }

  void TestUnion() {
    IntSet ua = *a_;
    ua.Union(*empty_);
    EXPECT_TRUE(a_->IsEqual(ua));
    IntSet ab = *a_;
    ab.Union(*b_);
    EXPECT_TRUE(a_->IsSubset(ab));
    EXPECT_TRUE(b_->IsSubset(ab));
    IntSet uall = *a_;
    uall.Union(*all_);
    EXPECT_TRUE(all_->IsEqual(uall));
  }

  void TestSet() {
    ValueType v = common_values[0];
    EXPECT_TRUE(a_->HasElement(v));
    IntSet old_a = *a_;
    a_->Add(v);
    EXPECT_TRUE(a_->IsEqual(old_a));
  }

  void TestHash() {
    size_t ha = a_->HashValue();
    size_t hb = b_->HashValue();
    CHECK_NE(ha, hb);
  }

  void TestIterator() {
    IntSet a(num_elements);
    ValueType i = 1;
    vector<ValueType> values;
    while (i < num_elements) {
      a.Add(i);
      values.push_back(i);
      i += i;
    }
    typename IntSet::Iterator iter(a);
    for (vector<ValueType>::const_iterator v = values.begin();
         v != values.end(); ++v) {
      EXPECT_FALSE(iter.Done());
      EXPECT_EQ(*v, iter.Value());
      iter.Next();
    }
    EXPECT_TRUE(iter.Done());
  }

 protected:
  void Init(int elements);
  typedef IntegerSet<uint64, max_elements> IntSet;
  typedef uint32 ValueType;
  IntSet *a_, *b_, *ab_, *empty_, *all_;
  int num_elements;
  vector<ValueType> common_values;
};

template<size_t max_elements>
void IntegerSetTest<max_elements>::Init(int elements) {
  ASSERT_LE(elements, max_elements);
  num_elements = elements;
  a_ = new IntSet(num_elements);
  b_ = new IntSet(num_elements);
  ab_ = new IntSet(num_elements);
  empty_ = new IntSet(num_elements);
  all_ = new IntSet(num_elements);

  common_values.clear();
  common_values.push_back(1);
  common_values.push_back(2);

  for (int i = 0; i < common_values.size(); ++i) {
    const ValueType v = common_values[i];
    ASSERT_LT(v, num_elements);
    a_->Add(v);
    b_->Add(v);
    ab_->Add(v);
  }
  a_->Add(0);
  b_->Add(num_elements - 1);
  for (ValueType p = 0; p < num_elements; ++p) {
    all_->Add(p);
  }
}

// Run all tests with a max. set size of N
template<size_t N>
void RunIntegerSetTest() {
  typedef IntegerSetTest<N> IntSetTest;
  IntSetTest t;
  t.TestAllSizes(&IntSetTest::TestSize);
  t.TestAllSizes(&IntSetTest::TestHasMember);
  t.TestAllSizes(&IntSetTest::TestIsEmpty);
  t.TestAllSizes(&IntSetTest::TestIsSubset);
  t.TestAllSizes(&IntSetTest::TestIsEqual);
  t.TestAllSizes(&IntSetTest::TestIntersect);
  t.TestAllSizes(&IntSetTest::TestSet);
  t.TestAllSizes(&IntSetTest::TestHash);
  t.TestAllSizes(&IntSetTest::TestIterator);
}

TEST(IntSet32Test, All) {
  RunIntegerSetTest<32>();
}

TEST(IntSet64Test, All) {
  RunIntegerSetTest<64>();
}

TEST(IntSet128Test, All) {
  RunIntegerSetTest<128>();
}

TEST(IntSet256Test, All) {
  RunIntegerSetTest<256>();
}

}  // namespace trainc
