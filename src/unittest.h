// unittest.h
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
// Unit testing using CppUnit

#ifndef UNITTEST_H_
#define UNITTEST_H_

#include <map>
#include <cassert>
#include <cppunit/TestCase.h>
#include <cppunit/TestCaller.h>
#include <cppunit/TestFixture.h>
#include <cppunit/TestSuite.h>
#include <cppunit/extensions/HelperMacros.h>
#include "fst/compat.h"

DECLARE_string(test_tmpdir);

#define STR(s) #s

// Define a test function N in TestSuite S.
#define TEST(S, N) \
  class TestCase_ ## S ## _ ## N : public CppUnit::TestCase { \
  public: \
    TestCase_ ## S ## _ ## N(std::string name) : CppUnit::TestCase(name) {} \
    virtual ~TestCase_ ## S ## _ ## N () {} \
    static CppUnit::Test *suite() { \
      CppUnit::TestSuite *suite = new CppUnit::TestSuite(STR(S ## _ ## N)); \
      suite->addTest(new TestCase_ ## S ## _ ## N("")); \
      return suite; \
    } \
    void runTest(); \
  }; \
  static testing::RegisterTestCase<TestCase_ ## S ## _ ## N> \
    Register_TestCase_ ## S ## _ ## N(STR(S), STR(N)); \
  void TestCase_ ## S ## _ ## N::runTest()

// Define a test case N for test fixture S.
#define TEST_F(S, N) \
  class TestFixture_ ## S ## _ ## N : public S { \
  public: \
    TestFixture_ ## S ## _ ## N () {} \
    virtual ~TestFixture_ ## S ## _ ## N () {} \
    void TEST_ ## N (); \
  }; \
  static testing::RegisterTest<TestFixture_ ## S ## _ ## N> \
    Register_TestFixture_ ## S ## _ ## N(\
        STR(S), STR(N), & TestFixture_ ## S ## _ ## N ::TEST_ ## N); \
  void TestFixture_ ## S ## _ ## N ::TEST_ ## N ()


#define EXPECT_TRUE(v) CPPUNIT_ASSERT(v)
#define EXPECT_FALSE(v) CPPUNIT_ASSERT(!(v))
#define EXPECT_EQ(x, y) CPPUNIT_ASSERT_EQUAL(x, y)
#define EXPECT_GE(x, y) CPPUNIT_ASSERT((x) >= (y))
#define EXPECT_NE(x, y) CPPUNIT_ASSERT((x) != (y))
#define EXPECT_GT(x, y) CPPUNIT_ASSERT((x) > (y))
#define EXPECT_LE(x, y) CPPUNIT_ASSERT((x) <= (y))
#define EXPECT_LT(x, y) CPPUNIT_ASSERT((x) < (y))

#define ASSERT_LE(x, y) assert((x) <= (y));
#define ASSERT_LT(x, y) assert((x) < (y));
#define ASSERT_GE(x, y) assert((x) >= (y));
#define ASSERT_GT(x, y) assert((x) > (y));
#define ASSERT_EQ(x, y) assert((x) == (y));
#define ASSERT_TRUE(x) assert(x);
#define ASSERT_NOTNULL(x) assert((x) != NULL);

namespace testing {

class Test : public CppUnit::TestFixture {
public:
  Test() {}
  virtual ~Test() {}

  virtual void setUp() { SetUp(); }
  virtual void tearDown() { TearDown(); }

protected:
  virtual void TearDown() {}
  virtual void SetUp() {}
};

// registry for all test cases.
class TestSuiteRegistry : public CppUnit::TestFactory {
public:
  typedef void (*TestMethod)();

  // return the only TestSuiteRegistry instance (singleton)
  static TestSuiteRegistry& instance() {
    if (!instance_) {
      instance_ = new TestSuiteRegistry();
      CppUnit::TestFactoryRegistry::getRegistry().registerFactory(instance_);
    }
    return *instance_;
  }

  // add a test case to the given test suite.
  bool addTest(const std::string &suiteName, CppUnit::Test *test) {
    if (suites_.find(suiteName) == suites_.end()) {
      suites_[suiteName] = new CppUnit::TestSuite(suiteName);
    }
    suites_[suiteName]->addTest(test);
    return true;
  }

  // generate a CppUnit test case including all registered test cases.
  CppUnit::Test* makeTest() {
    CppUnit::TestSuite *allTests = new CppUnit::TestSuite("All");
    for (SuiteMap::const_iterator i = suites_.begin();
        i != suites_.end(); ++i) {
      allTests->addTest(i->second);
    }
    return allTests;
  }
protected:
  typedef std::map<std::string, CppUnit::TestSuite*> SuiteMap;
  SuiteMap suites_;

private:
  TestSuiteRegistry() {}
  static TestSuiteRegistry *instance_;
};

// adds a test case to the registry.
// to be used as static object.
template<class T>
class RegisterTest {
public:
  RegisterTest(const std::string &suiteName,
               const std::string &testName, void (T::*m)())
    : registry_(TestSuiteRegistry::instance()) {
    registry_.addTest(suiteName, new CppUnit::TestCaller<T>(testName, m));
  }
protected:
  RegisterTest() : registry_(TestSuiteRegistry::instance()) {}
  TestSuiteRegistry &registry_;
};


// adds a test fixture to the registry.
// to be used as static object.
template<class T>
class RegisterTestCase : public RegisterTest<T> {
public:
  RegisterTestCase(const std::string &suiteName, const std::string &testName) {
    this->registry_.addTest(suiteName, new T(testName));
  }
};

}  // namespace testing

#endif  // UNITTEST_H_
