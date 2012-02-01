// unittest.cc
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
// main function for executing all registered or selected unit tests.

#include <cppunit/TestFailure.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TextTestProgressListener.h>
#include <cppunit/TextOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include "fst/compat.h"
#include "unittest.h"

DEFINE_string(test_tmpdir, "", "directory for intermediate test data");
DEFINE_int32(num_threads, 1, "number of threads used");

testing::TestSuiteRegistry* testing::TestSuiteRegistry::instance_ = 0;

class ProgressListener: public CppUnit::TestListener {
public:
  ProgressListener() {}
  virtual ~ProgressListener() {
  }

  virtual void startSuite(CppUnit::Test *suite) {
    std::cerr << "" << suite->getName() << " ("
              << suite->getChildTestCount() << ")" << std::endl;
  }

  virtual void startTest(CppUnit::Test *test) {
    std::cerr << "    "
              << (test->getName().empty() ? "n/a" : test->getName())
              << std::endl;
    curTestFailure_ = false;
  }

  virtual void addFailure(const CppUnit::TestFailure &failure) {
    curTestFailure_ = true;
  }

  virtual void endTest(CppUnit::Test *test) {
    std::cerr << "        => "
              << (curTestFailure_ ? "FAILED" : "OK") << std::endl;
  }

private:
  ProgressListener(const ProgressListener &copy );
  void operator =(const ProgressListener &copy);
  bool curTestFailure_;
};

CppUnit::Test* findTest(CppUnit::Test *root, const std::string &name) {
  std::deque<CppUnit::Test*> to_visit;
  to_visit.push_back(root);
  while (!to_visit.empty()) {
    CppUnit::Test *t = to_visit.front();
    to_visit.pop_front();
    if (t->getName() == name) return t;
    for (int i = 0; i < t->getChildTestCount(); ++i) {
      to_visit.push_back(t->getChildTestAt(i));
    }
  }
  return NULL;
}

std::string getTempDir() {
  char *t = std::getenv("TMPDIR");
  std::string tmpdir;
  if (t)
    tmpdir = t;
  else
    tmpdir = "/tmp";
  const size_t len = tmpdir.length() + 8;
  char *dir_template = new char[len];
  snprintf(dir_template, len, "%s/XXXXXX", tmpdir.c_str());
  std::string result = ::mkdtemp(dir_template);
  delete[] dir_template;
  return result;
}

int main(int argc, char **argv) {
  SetFlags("", &argc, &argv, true);
  if (FLAGS_test_tmpdir.empty()) {
    FLAGS_test_tmpdir = getTempDir();
    VLOG(1) << "using test temp dir " << FLAGS_test_tmpdir;
  }
  CppUnit::TestResult controller;
  CppUnit::TestResultCollector result;
  controller.addListener(&result);
  ProgressListener progressListener;
  controller.addListener(&progressListener);
  CppUnit::TestRunner runner;
  CppUnit::TestFactoryRegistry &registry =
      CppUnit::TestFactoryRegistry::getRegistry();
  CppUnit::Test *test = registry.makeTest();
  for (int i = 1; i < argc; ++i) {
    test = findTest(test, argv[i]);
    if (!test) {
      LOG(FATAL) << "Test '" << argv[1] << "' not found";
    }
  }
  runner.addTest(test);
  runner.run(controller);
  CppUnit::TextOutputter output(&result, std::cout);
  output.write();
  return 0;
}

