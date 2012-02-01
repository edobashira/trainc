// context_set.cc
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
// Copyright 2010 Google Inc. All Rights Reserved.
// Author: rybach@google.com (David Rybach)

#include <sstream>
#include "context_set.h"
#include "hash.h"

namespace trainc {

string PhoneContext::ToString() const {
  std::stringstream ss;
  for (int p = -NumLeftContexts(); p <= NumRightContexts(); ++p) {
    const ContextSet &set = contexts_[ContextPositionToIndex(p)];
    ss << "{";
    for (ContextSet::Iterator p(set); !p.Done(); p.Next())
      ss << p.Value() << " ";
    ss << "} ";
  }
  return ss.str();
}

}  // namespace trainc
