// recipe.cc
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
//

#include <stdint.h>
#include <iterator>
#include <algorithm>
#include "recipe.h"
#include "model_splitter.h"

namespace trainc {

namespace {
template<class C>
void WriteContainer(OutputBuffer *o, const C &v) {
  typedef typename C::const_iterator Iter;
  int n = std::distance(v.begin(), v.end());
  o->WriteBinary(n);
  for (Iter i = v.begin(); i != v.end(); ++i)
    o->WriteBinary(*i);
}

template<class C>
bool ReadContainer(InputBuffer *i, C *v) {
  DCHECK(v->empty());
  int n = 0;
  if (!i->ReadBinary(&n))
    return false;
  typedef typename C::value_type Value;
  std::insert_iterator<C> insert = std::inserter(*v, v->begin());
  for (int j = 0; j < n; ++j) {
    Value value;
    if (!i->ReadBinary(&value))
      return false;
    insert = value;
  }
  DCHECK_EQ(std::distance(v->begin(), v->end()), n);
  return true;
}
}  // namespace

template<>
void OutputBuffer::WriteBinary(const ContextSet &c) {
  WriteBinary<int>(c.Capacity());
  WriteBinary<int>(c.Size());
  for (ContextSet::Iterator i(c); !i.Done(); i.Next())
    WriteBinary(i.Value());
}

template<>
bool InputBuffer::ReadBinary(ContextSet *c) {
  int cap = 0, n = 0;
  if (!(ReadBinary(&cap) && ReadBinary(&n)))
    return false;
  *c = ContextSet(cap);
  for (int i = 0; i < n; ++i) {
    ContextSet::ValueType value;
    if (!ReadBinary(&value))
      return false;
    c->Add(value);
  }
  return true;
}

template<>
void OutputBuffer::WriteBinary(const PhoneContext &c) {
  WriteBinary(c.NumLeftContexts());
  WriteBinary(c.NumRightContexts());
  for (int p = -c.NumLeftContexts(); p <= c.NumRightContexts(); ++p)
    WriteBinary(c.GetContext(p));
}

template<>
bool InputBuffer::ReadBinary(PhoneContext *c) {
  int nl = 0, nr = 0;
  if (!(ReadBinary(&nl) && ReadBinary(&nr)))
    return false;
  ContextSet value(0);
  *c = PhoneContext(0, nl, nr);
  for (int p = -nl; p <= nr; ++p) {
    if (!ReadBinary(&value))
      return false;
    c->SetContext(p, value);
  }
  return true;
}

template<>
void OutputBuffer::WriteBinary(const AllophoneModelStub &m) {
  WriteContainer(this, m.phones);
}

template<>
bool InputBuffer::ReadBinary(AllophoneModelStub *m) {
  return ReadContainer(this, &m->phones);
}

template<>
void OutputBuffer::WriteBinary(const AllophoneStateModelStub &m) {
  WriteBinary(m.state);
  WriteBinary(m.context);
  WriteContainer(this, m.allophones);
}

template<>
bool InputBuffer::ReadBinary(AllophoneStateModelStub *m) {
  return ReadBinary(&m->state) && ReadBinary(&m->context) &&
      ReadContainer(this, &m->allophones);
}

template<>
void OutputBuffer::WriteBinary(const SplitDef &def) {
  WriteBinary(def.position);
  WriteBinary(def.question);
  WriteBinary(def.model);
}

template<>
bool InputBuffer::ReadBinary(SplitDef *def) {
  return ReadBinary(&def->position) && ReadBinary(&def->question) &&
      ReadBinary(&def->model);
}


AllophoneStateModelStub::AllophoneStateModelStub(const AllophoneStateModel &m)
    : state(m.state()), context(m.GetContext()) {
  typedef AllophoneList::const_iterator Iter;
  const AllophoneList &list = m.GetAllophones();
  for (Iter a = list.begin(); a != list.end(); ++a)
    allophones.push_back(AllophoneModelStub(**a));
}

bool AllophoneStateModelStub::IsEqual(const AllophoneStateModel &m) const {
  if (state != m.state() || !context.IsEqual(m.GetContext()) ||
      allophones.size() != m.GetAllophones().size())
    return false;
  const AllophoneList &list = m.GetAllophones();
  std::vector<AllophoneModelStub>::const_iterator i = allophones.begin();
  for (AllophoneList::const_iterator a = list.begin(); a != list.end(); ++a, ++i)
    if (!i->IsEqual(**a)) return false;
  return true;
}

const RecipeWriter::Header RecipeWriter::kHeader = 0x52435054;
const int RecipeWriter::kVersion = 1;

bool RecipeWriter::Init() {
  out_.WriteBinary(kHeader);
  out_.WriteBinary(kVersion);
  return true;
}

void RecipeWriter::AddSplit(const SplitHypothesis &split) {
  SplitDef def;
  def.position = split.position;
  def.question = GetQuestionId(split.position, split.question);
  def.model = AllophoneStateModelStub(**split.model);
  out_.WriteBinary(def);
}

int RecipeWriter::GetQuestionId(int pos, const ContextQuestion *question) const {
  const QuestionSet &questions = *questions_->at(num_left_contexts_ + pos);
  QuestionSet::const_iterator i =
      std::find(questions.begin(), questions.end(), question);
  DCHECK(i != questions.end());
  return std::distance(questions.begin(), i);
}

bool RecipeReader::Init() {
  int header, version;
  if (!(in_.ReadBinary(&header) && in_.ReadBinary(&version)))
    return false;
  return (header == RecipeWriter::kHeader &&
      version == RecipeWriter::kVersion);
}

bool RecipeReader::ReadSplit(SplitDef *split) {
  return in_.ReadBinary(split);
}

ModelSplitter::SplitHypRef ReplaySplitter::FindBestSplit() {
  CHECK_NOTNULL(reader_);
  SplitDef def;
  if (!reader_->ReadSplit(&def)) {
    return split_hyps_.end();
  }
  const ContextBuilder::QuestionSet &questions =
      *questions_[num_left_contexts_ + def.position];
  DCHECK_LT(def.question, questions.size());
  const ContextQuestion *question = questions[def.question];
  SplitHypRef result = split_hyps_.end();
  for (SplitHypotheses::const_iterator hyp = split_hyps_.begin(); hyp != split_hyps_.end(); ++hyp) {
    if (hyp->position == def.position && hyp->question == question && def.model.IsEqual(**hyp->model)) {
      DCHECK(result == split_hyps_.end());
      result = hyp;
      // TODO: enable break
      // break;
    }
  }
  if (result == split_hyps_.end())
    LOG(FATAL) << "split not found";
  return result;
}

}  // namespace trainc



