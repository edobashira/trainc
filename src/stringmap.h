// stringmap.h
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
// string to string map

#ifndef STRINGMAP_H_
#define STRINGMAP_H_

#include <map>
#include <string>

namespace trainc {

// Simple string to string map based on std::map.
// Supports loading a map from a text file.
class StringMap : public std::map<std::string, std::string> {
  typedef std::map<std::string, std::string> Map;
public:
  StringMap() {}

  // load a string to string mapping from file.
  // the text file has one mapping per line separated by space.
  bool LoadMap(const std::string &file);

  // returns the mapped value for the given key.
  // returns an empty string if key is not found.
  const std::string& get(const std::string &key) const;

private:
  static const std::string kDefaultValue;
};

}  // namespace trainc

#endif  // STRINGMAP_H_
