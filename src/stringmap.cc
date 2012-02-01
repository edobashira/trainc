// stringmap.cc
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

#include "stringmap.h"
#include "file.h"
#include "stringutil.h"
#include "util.h"

namespace trainc {

const std::string StringMap::kDefaultValue = "";

bool StringMap::LoadMap(const std::string &file) {
  File *f = File::Create(file, "r");
  if (!f) return false;
  CHECK(f->Open());
  InputBuffer ib(f);
  std::string buffer;
  std::vector<std::string> items;
  while (ib.ReadLine(&buffer)) {
    items.clear();
    SplitStringUsing(buffer, " ", &items);
    if (items.size() > 2) {
      std::string value;
      JoinStringsUsing(items.begin() + 1, items.end(), " ", &value);
      insert(std::make_pair(items[0], value));
    } else if (items.size() > 1) {
      insert(std::make_pair(items[0], items[1]));
    }
  }
  return true;
}

const std::string& StringMap::get(const std::string &key) const {
  Map::const_iterator i = find(key);
  if (i == end()) return kDefaultValue;
  else return i->second;
}

}
