// set_inventory.h
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
// Phonetic question set

#ifndef SET_INVENTORY_H_
#define SET_INVENTORY_H_

#include <vector>
#include "file.h"
#include "stringutil.h"
#include "util.h"

namespace fst {
class SymbolTable;
}

namespace trainc {

class InventoryIterator;

// A set of phone sets, used for phonetic questions.
// File format:
//   question-1 phone-1 phone-2
//   question-2 phone-2 phone-3
// phones must be valid symbols (according to the symbol table used)
class SetInventory {
public:
  SetInventory() : symbols_(NULL) {}

  ~SetInventory() {
    delete symbols_;
  }

  void SetSymTable(const fst::SymbolTable &symbols) {
    delete symbols_;
    symbols_ = symbols.Copy();
  }

  const fst::SymbolTable* GetSymTable() const { return symbols_; }

  // read the sets from a file
  void ReadText(const std::string &filename) {
    CHECK_NOTNULL(symbols_);
    File *f = File::OpenOrDie(filename, "r");
    InputBuffer ib(f);
    std::string line;
    while (ib.ReadLine(&line)) {
      std::vector<std::string> items;
      SplitStringUsing(line, " ", &items);
      CHECK_GT(items.size(), 1);
      std::vector<int> &p = sets_[items[0]];
      VLOG(3) << "question " << items[0] << ":";
      for (std::vector<std::string>::const_iterator i = items.begin() + 1;
          i != items.end(); ++i) {
        p.push_back(symbols_->Find(*i));
        VLOG(3) << " " << *i << "=" << symbols_->Find(*i);
      }
    }
  }

  // number of sets / questions
  int NumSets() { return sets_.size(); }

  // return one set of quetions
  const std::vector<int>& GetSet(const std::string &name) const {
    if (sets_.find(name) == sets_.end())
      return empty_set_;
    else
      return sets_.find(name)->second;
  }

  friend class InventoryIterator;
private:
  fst::SymbolTable *symbols_;
  typedef std::map< std::string, std::vector<int> > SetMap;
  SetMap sets_;
  std::vector<int> empty_set_;
};

// iterator over the phones of one phone set
class ContextSetIterator {
public:
  ContextSetIterator(const std::vector<int> &v) : iter_(v.begin()), end_(v.end()) {}
  bool Done() const { return iter_ == end_; }
  void Next() { ++iter_; }
  int Value() const { return *iter_; }
private:
  std::vector<int>::const_iterator iter_, end_;
};

// iterator for over all phone sets
class InventoryIterator {
public:
  InventoryIterator(const SetInventory *inv) : iter_(inv->sets_.begin()), end_(inv->sets_.end()) {}
  bool Done() const { return iter_ == end_; }
  void Next() { ++iter_; }
  const std::string& Name() const { return iter_->first; }
  ContextSetIterator Value() const { return ContextSetIterator(iter_->second); }
private:
  SetInventory::SetMap::const_iterator iter_, end_;

};


}  // namespace trainc

#endif  // 
