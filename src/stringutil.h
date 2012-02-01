// stringutil.h
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
// String utility functions

#ifndef STRINGUTIL_H_
#define STRINGUTIL_H_

#include <cstdarg>
#include <vector>
#include <string>

namespace trainc {

// Remove trailing and leading white space from the given string.
void StripWhiteSpace(std::string *s);

// Split a string into a vector of strings using the given separator.
void SplitStringUsing(const std::string &to_split,
                      const std::string &seperator,
                      std::vector<std::string> *result);

// Join a sequence of strings to one string, inserting the
// separator string between all items.
void JoinStringsUsing(std::vector<std::string>::const_iterator begin,
                      std::vector<std::string>::const_iterator end,
                      const std::string &separator,
                      std::string *result);

// Join a sequence of strings to one string, inserting the
// separator string between all items.
void JoinStringsUsing(const std::vector<std::string> &to_join,
                     const std::string &separator,
                     std::string *result);

// printf returning a std::string.
std::string StringPrintf(const char *format, ...);

// printf returning a std::string.
std::string StringVPrintf(const char *format, va_list ap);

}

#endif /* STRINGUTIL_H_ */
