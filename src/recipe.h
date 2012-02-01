// recipe.h
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
// Serialization of splits

#ifndef EPERIMENTAL_SPEECH_CONTEXT_BUILDER_RECIPE_H_
#define EPERIMENTAL_SPEECH_CONTEXT_BUILDER_RECIPE_H_

#include <vector>
#include "context_builder.h"
#include "file.h"
#include "model_splitter.h"
#include "phone_models.h"

namespace trainc {

struct SplitHypothesis;

class AllophoneModelStub {
public:
  AllophoneModelStub() {}
  explicit AllophoneModelStub(const AllophoneModel &m) : phones(m.phones()) {}
  bool IsEqual(const AllophoneModel &o) const {
    return o.phones() == phones;
  }
  std::vector<int> phones;
};

class AllophoneStateModelStub {
public:
  AllophoneStateModelStub() : context(0, 0, 0) {}
  explicit AllophoneStateModelStub(const AllophoneStateModel &m);
  bool IsEqual(const AllophoneStateModel &model) const;
  std::vector<AllophoneModelStub> allophones;
  int state;
  PhoneContext context;
private:
  typedef AllophoneStateModel::AllophoneRefList AllophoneList;
};

struct SplitDef {
  int question, position;
  AllophoneStateModelStub model;
};

class RecipeWriter {
  typedef ContextBuilder::QuestionSet QuestionSet;
public:
  explicit RecipeWriter(File *file)
      : out_(file), num_left_contexts_(0), questions_(NULL) {}
  void SetQuestions(int num_left_contexts,
                    const std::vector<const QuestionSet*> *questions) {
    num_left_contexts_ = num_left_contexts;
    questions_ = questions;
  }
  bool Init();
  void AddSplit(const SplitHypothesis &split);

  typedef uint32_t Header;
  static const Header kHeader;
  static const int kVersion;
protected:
  int GetQuestionId(int pos, const ContextQuestion *question) const;
  OutputBuffer out_;
  int num_left_contexts_;
  const std::vector<const QuestionSet*> *questions_;

};

class RecipeReader {
public:
  explicit RecipeReader(File *file) : in_(file) {}
  bool Init();
  bool ReadSplit(SplitDef *split);
protected:
  InputBuffer in_;
};

class ReplaySplitter : public ModelSplitter {
public:
  ReplaySplitter() : reader_(NULL) {}
  virtual ~ReplaySplitter() {
    delete reader_;
  }
  bool SetFile(File *file) {
    reader_ = new RecipeReader(file);
    return reader_->Init();
  }

protected:
  virtual SplitHypRef FindBestSplit();
private:
  RecipeReader *reader_;
};

}  // namespace trainc

#endif  // EPERIMENTAL_SPEECH_CONTEXT_BUILDER_SERIALIZATION_H_
