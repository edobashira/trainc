// transducer_test.h
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
// class definition of ConstructionalTransducerTest.
// defined in a separate header file, because it ConstructionalTransducerTest
// is also used in composed_transducer_test.cc

#ifndef TRANSDUCER_TEST_H_
#define TRANSDUCER_TEST_H_

#include <map>
#include "transducer.h"
#include "unittest.h"
using std::map;

namespace trainc {

class ModelManager;
class Phones;

class ConstructionalTransducerTest : public ::testing::Test {
 protected:
  ConstructionalTransducerTest()
      : c_(NULL), models_(NULL),
        phone_info_(NULL), all_phones_(NULL) {}
  virtual void SetUp() {}
  virtual void TearDown();

  virtual void Init(int num_phones, int num_left_contexts,
                    int num_right_contexts, bool center_set = false);
  void SplitOneModel(int position);
  void SplitAllModels(int position, ContextSet *s = NULL);
  void SplitIndividual(int niter, int nquestions, bool check_counts);
  void VerifyModels();
  virtual void VerifyTransducer();
  virtual void InitTransducer();
  virtual void InitSharedStateTransducer();
  void CreatePhoneSets(ContextSet *a, ContextSet *b);
  virtual StateCountingTransducer* GetC() {
    return c_;
  }


  ConstructionalTransducer *c_;
  ModelManager *models_;
  Phones *phone_info_;
  ContextSet *all_phones_;
  int num_phones_;
  int num_left_contexts_;
  int num_right_contexts_;
  bool center_set_;
  map<int, int> phone_mapping_;
};


}  // namespace trainc

#endif  // TRANSDUCER_TEST_H_
