// stringutil.cc
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

#include "stringutil.h"
#include <cstdio>
#include <algorithm>
#include <iterator>
#include <sstream>

using std::string;
using std::vector;

namespace trainc {

void StripWhiteSpace(string *s) {
  string::size_type i;
  const char kWhitespace[] = " \t\n\r\f\v" ;
  i = s->find_first_not_of(kWhitespace) ;
  if (i == string::npos) i = s->length();
  s->erase(0, i) ;
  i = s->find_last_not_of(kWhitespace) ;
  if (i != string::npos) s->erase(i + 1) ;
}


void SplitStringUsing(const string &to_split,
                      const string &separator,
                      vector<string> *result) {
  if (to_split.empty())
    return;
  string::size_type start = 0;
  string::size_type end = 0;
  while (end != string::npos) {
    end = to_split.find(separator, start);
    if (end == string::npos) {
      result->push_back(to_split.substr(start));
      StripWhiteSpace(&result->back());
    } else {
      if (end > start) {
        result->push_back(to_split.substr(start, end - start));
        StripWhiteSpace(&result->back());
      }
      start = end + separator.size();
    }
  }
}

void JoinStringsUsing(const vector<string>::const_iterator begin,
                      const vector<string>::const_iterator end,
                      const string &separator,
                      string *result) {
  std::stringstream buf;
  std::copy(begin, end, std::ostream_iterator<string>(buf, separator.c_str()));
  *result = buf.str();
  result->erase(result->length() - separator.size());
}

void JoinStringsUsing(const vector<string> &to_join,
                      const string &separator,
                      string *result) {
  return JoinStringsUsing(to_join.begin(), to_join.end(), separator, result);
}

string StringPrintf(const char *format, ...) {
  va_list ap;
  size_t buf_size = 0;
  char *buf = NULL;
  int n;
  for (;;) {
    va_start(ap, format);
    n = vsnprintf(buf, buf_size, format, ap);
    va_end(ap);
    if (n < 0) {
      // until glibc 2.0.6 they would return -1 when the output was truncated.
      // double the buffer size
      n = (buf_size > 64) ? 2 * buf_size : 64;
    } else {
      // Newer implementations (>= glibc 2.1) return the number
      // of characters needed.
      if (n + 1 <= buf_size)
        break; // +1 for terminating null byte
    }
    if (buf) delete[] buf;
    buf = new char[buf_size = n + 1];
  }
  string result(buf, n);
  delete[] buf;
  return result;
}

string StringVPrintf(const char *format, va_list ap) {
  size_t buf_size = 0;
  char *buf = 0;
  int n;
  va_list ap2;
  for (;;) {
    va_copy(ap2, ap);
    n = vsnprintf(buf, buf_size, format, ap2);
    if (n < 0) {
      // until glibc 2.0.6 they would return -1 when the output was truncated.
      // double the buffer size
      n = (buf_size > 64) ? 2 * buf_size : 64;
    } else {
      // Newer implementations (>= glibc 2.1) return the number
      // of characters needed.
      if (n + 1 <= buf_size)
        break; // +1 for terminating null byte
    }
    delete[] buf;
    buf = new char[buf_size = n + 1];
  }
  std::string result(buf, n);
  delete[] buf;
  return result;
}

}  // namespace trainc
