// thread_test.cc
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
// Copyright 2011 RWTH Aachen University. All Rights Reserved.
// Author: rybach@cs.rwth-aachen.de (David Rybach)
//
// \file
// Test functions for multi-threading functions

#include <unistd.h>
#include "thread.h"
#include "unittest.h"

namespace threads {

class Shared {
private:
  mutable Mutex lock_;
  int a_;
public:
  Shared() : a_(0) {}
  void Inc() {
    lock_.Lock();
    ++a_;
    lock_.Release();
  }

  int Get() const {
    int r;
    lock_.Lock();
    r = a_;
    lock_.Release();
    return r;
  }
};

class TestThread : public Thread {
public:
public:
  TestThread(Shared *data) : data_(data) {}
protected:
  virtual void Run() {
    timeval now;
    gettimeofday(&now, 0);
    ::usleep(now.tv_usec);
    data_->Inc();
  }

  Shared *data_;
};

TEST(ThreadTest, Simple) {
  const int num_thread = 10;
  std::vector<TestThread*> threads;
  Shared data;
  for (int t = 0; t < num_thread; ++t)
    threads.push_back(new TestThread(&data));
  for (int t = 0; t < num_thread; ++t)
    threads[t]->Start();
  for (int t = 0; t < num_thread; ++t)
    threads[t]->Wait();
  for (int t = 0; t < num_thread; ++t)
    delete threads[t];
  EXPECT_EQ(data.Get(), num_thread);
}

class TestTask {
public:
  explicit TestTask(int v = 0) { value = v; }
  int value;
  void Set(int v) { value = v; }
};

class TestMapper {
public:
  TestMapper* Clone() const { return new TestMapper(); }
  void Map(const TestTask &task) {
    sum += task.value;
  }
  void Reset() {
    sum = 0;
  }
  int sum;
};

class TestPtrMapper {
public:
  TestPtrMapper* Clone() const { return new TestPtrMapper(); }
  void Map(const TestTask *task) {
    sum += task->value;
    delete task;
  }
  void Reset() {
    sum = 0;
  }
  int sum;
};

class TestPtrModifyMapper {
public:
  TestPtrModifyMapper* Clone() const { return new TestPtrModifyMapper(); }
  void Map(TestTask *task) {
    task->Set(1);
    sum += task->value;
  }
  void Reset() {
    sum = 0;
  }
  int sum;
};

class TestReducer {
public:
  TestReducer() { sum = 0; }
  void Reduce(TestMapper *mapper) {
    sum += mapper->sum;
  }
  void Reduce(TestPtrMapper *mapper) {
    sum += mapper->sum;
  }
  void Reduce(TestPtrModifyMapper *mapper) {
    sum += mapper->sum;
  }
  int sum;
};

TEST(ThreadPoolTest, Simple) {
  ThreadPool<TestTask, TestMapper, TestReducer> pool;
  TestMapper mapper;
  pool.Init(10, mapper);
  const int num_task = 100;
  for (int t = 0; t < num_task; ++t) {
    pool.Submit(TestTask(t));
  }
  TestReducer reducer;
  pool.Combine(&reducer);
  VLOG(2) << "sum_: " << reducer.sum;
  EXPECT_EQ(reducer.sum, num_task*(num_task - 1)/2);
}

TEST(ThreadPoolTest, Pointer) {
  ThreadPool<TestTask*, TestPtrMapper, TestReducer> pool;
  TestPtrMapper mapper;
  pool.Init(10, mapper);
  const int num_task = 100;
  for (int t = 0; t < num_task; ++t) {
    pool.Submit(new TestTask(t));
  }
  TestReducer reducer;
  pool.Combine(&reducer);
  VLOG(2) << "sum_: " << reducer.sum;
  EXPECT_EQ(reducer.sum, num_task*(num_task - 1)/2);
}

TEST(ThreadPoolTest, Modify) {
  ThreadPool<TestTask*, TestPtrModifyMapper, TestReducer> pool;
  TestPtrModifyMapper mapper;
  pool.Init(10, mapper);
  const int num_task = 100;
  for (int t = 0; t < num_task; ++t) {
    pool.Submit(new TestTask(t));
  }
  TestReducer reducer;
  pool.Combine(&reducer);
  VLOG(2) << "sum_: " << reducer.sum;
  EXPECT_EQ(reducer.sum, num_task);
}

TEST(ThreadPoolTest, Reset) {
  ThreadPool<TestTask, TestMapper, TestReducer> pool;
  TestMapper mapper;
  pool.Init(10, mapper);
  int num_task = 100;
  for (int t = 0; t < num_task; ++t) {
    pool.Submit(TestTask(t));
  }
  pool.Wait();
  pool.Reset();
  num_task = 10;
  for (int t = 0; t < num_task; ++t) {
    pool.Submit(TestTask(t));
  }
  TestReducer reducer;
  pool.Combine(&reducer);
  VLOG(2) << "sum_: " << reducer.sum;
  EXPECT_EQ(reducer.sum, num_task*(num_task - 1)/2);
}

}  // namespace threads

