// sample_reader.h
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
// Read training samples from file

#ifndef SAMPLE_READER_H_
#define SAMPLE_READER_H_

#include <fstream>

namespace fst {
class SymbolTable;
}

namespace trainc {

class Samples;
class Statistics;

// base class for all sample readers and factory function.
// SetPhoneSymbols() has to be called before Read()
class SampleReader {
public:
  SampleReader() : phone_symbols_(NULL) {}
  virtual ~SampleReader() {}

  // Set the symbol table.
  // Ownership stays at caller.
  void SetPhoneSymbols(const fst::SymbolTable *symbols) {
    phone_symbols_ = symbols;
  }
  virtual bool Read(const std::string &filename, Samples *samples) = 0;

  static SampleReader* Create(const std::string &type);

protected:
  const fst::SymbolTable *phone_symbols_;
};

// read samples from a simple text file.
// file format:
// header:
//   version feature-dimension num-left-contexts num-right-contexts
// one sample per line:
//   <hmm_definition> <context_definition> <statistics>
// with
//   <hmm_definition> := <phone> <hmm_state>
//   <context_definition> := <context> <context>
//   <context> := <phone> ... <phone>
//   <statistics> := <weight> <sum> <sum>
//   <sum> := <value> ... <value>
//   <weight> := float
//   <value> := float
//   <phone> := string
//   <hmm_state> := int
//
// <context> is always from leftmost context phone to right
// example: state 1 of phone "A" with left context "B C"
// and right context "D E"
//   A 1 B C D E ....
//
// <statistics> is the number of observations, the sum of observed features,
// and the sum of squared features.
class SampleTextReader : public SampleReader {
public:
  virtual ~SampleTextReader() {}
  virtual bool Read(const std::string &filename, Samples *samples);
  static std::string name() { return "text"; }

protected:
  bool ReadHeader(std::ifstream *fin);
  bool ReadSample(std::ifstream *fin, Samples *samples) const;
  bool ReadStatistics(std::ifstream *fin, Statistics *stats) const;
  template<class I>
  bool ReadPhoneSequence(std::ifstream *fin, I begin, I end) const;
  static const int kFormatVersion;
  int dimension_, num_left_contexts_, num_right_contexts_;
};

}  // namespace trainc

#endif  // CONTEXT_
