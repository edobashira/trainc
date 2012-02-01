// sample_reader.cc
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

#include <fstream>
#include "fst/symbol-table.h"
#include "sample_reader.h"
#include "sample.h"

namespace trainc {

SampleReader* SampleReader::Create(const std::string &name) {
  // default reader
  return new SampleTextReader();
}

const int SampleTextReader::kFormatVersion = 1;

bool SampleTextReader::Read(const std::string &filename, Samples *samples) {
  CHECK_NOTNULL(phone_symbols_);
  CHECK_GT(samples->NumPhones(), 0);
  VLOG(1) << "reading samples from: " << filename;
  std::ifstream fin(filename.c_str());
  if (!fin) {
    LOG(ERROR) << "cannot open " << filename;
    return false;
  }
  if (!ReadHeader(&fin)) {
    LOG(ERROR) << "error reading header";
    return false;
  }
  samples->SetFeatureDimension(dimension_);
  int line = 2;
  while (!fin.eof()) {
    if (!ReadSample(&fin, samples)) {
      LOG(ERROR) << "error reading sample in line " << line;
      return false;
    }
    ++line;
  }
  VLOG(1) << "read samples: " << (line - 2);
  return true;
}

bool SampleTextReader::ReadHeader(std::ifstream *fin) {
  std::string line;
  int version;
  *fin >> version >> dimension_ >> num_left_contexts_ >> num_right_contexts_;
  if (!fin->good()) return false;
  return (version == kFormatVersion);
}

template<class Iterator>
bool SampleTextReader::ReadPhoneSequence(std::ifstream *fin,
                                         Iterator begin, Iterator end) const {
  std::string symbol;
  for (; begin != end; ++begin) {
    *fin >> symbol;
    *begin = phone_symbols_->Find(symbol);
    if (fin->fail() || *begin < 0) return false;
  }
  return true;
}

bool SampleTextReader::ReadSample(std::ifstream *fin, Samples *samples) const {
  std::string sym;
  int state;
  *fin >> sym;
  if (fin->eof()) return true;
  *fin >> state;
  int phone = phone_symbols_->Find(sym);
  if (phone < 0 || state < 0) return false;
  Sample *sample = samples->AddSample(phone, state);
  sample->left_context_.resize(num_left_contexts_);
  sample->right_context_.resize(num_right_contexts_);
  if (!(ReadPhoneSequence(fin, sample->left_context_.rbegin(),
                          sample->left_context_.rend()) &&
        ReadPhoneSequence(fin, sample->right_context_.begin(),
                          sample->right_context_.end()))) {
    return false;
  }
  return ReadStatistics(fin, &sample->stat);
}

bool SampleTextReader::ReadStatistics(std::ifstream *fin, Statistics *stats) const {
  float weight;
  *fin >> weight;
  stats->SetWeight(weight);
  float *s = stats->SumRef();
  for (int d = 0; d < dimension_; ++d, ++s) {
    *fin >> *s;
  }
  s = stats->Sum2Ref();
  for (int d = 0; d < dimension_; ++d, ++s) {
    *fin >> *s;
  }
  return !fin->fail();
}

}  // namespace trainc
