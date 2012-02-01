// debug.h
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
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Google-style debugging macro definitions

#ifndef DEBUG_H_
#define DEBUG_H_

#include "util.h"

#ifdef DEBUG

#define DCHECK(x) CHECK(x)
#define DCHECK_EQ(x, y) CHECK_EQ(x, y)
#define DCHECK_NE(x, y) CHECK_NE(x, y)
#define DCHECK_LE(x, y) CHECK_LE(x, y)
#define DCHECK_LT(x, y) CHECK_LT(x, y)
#define DCHECK_GE(x, y) CHECK_GE(x, y)
#define DCHECK_GT(x, y) CHECK_GT(x, y)

#else

#define DCHECK(x)
#define DCHECK_EQ(x, y)
#define DCHECK_NE(x, y)
#define DCHECK_LT(x, y)
#define DCHECK_LE(x, y)
#define DCHECK_GE(x, y)
#define DCHECK_GT(x, y)

#endif  // DEBUG

#endif  // DEBUG_H_
