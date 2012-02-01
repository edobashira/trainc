// array.h
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
// Wrapper class for a basic fixed-size Array

#ifndef ARRAY_H_
#define ARRAY_H_

#include <algorithm>
#include "debug.h"

// Fixed size array.
template<class T, size_t max_size>
class Array {
public:
  typedef size_t SizeType;
  typedef T ValueType;
  typedef T* ArrayType;

public:
  Array(SizeType size = 0, const ValueType &init = T())
    : size_(size) {
    CHECK_LE(size_, max_size);
    std::fill(data_, data_ + size_, init);
  }

  const ValueType& operator[](SizeType i) const {
    DCHECK_LT(i, size_);
    return data_[i];
  }

  ValueType& operator[](SizeType i) {
    DCHECK_LT(i, size_);
    return data_[i];
  }

  const T* const array() const { return data_; }

  ArrayType mutable_array() { return data_; }

protected:
  size_t size_;
  T data_[max_size];
};  // class Array

#endif  // 
