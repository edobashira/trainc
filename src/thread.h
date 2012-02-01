// thread.h
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
// multi-threading related classes using pthreads

#ifndef THREAD_H_
#define THREAD_H_

#include <pthread.h>
#include <sys/time.h>
#include <ctime>
#include <vector>
#include <deque>
#include "util.h"

#ifndef PTHREAD_MUTEX_RECURSIVE_NP
// PTHREAD_MUTEX_RECURSIVE_NP is not available on some platforms
#define PTHREAD_MUTEX_RECURSIVE_NP PTHREAD_MUTEX_RECURSIVE
#endif

namespace threads {

// Abstract Thread.
// Thread implementation based on pthreads
class Thread {
public:
  Thread() :
    running_(false) {}

  virtual ~Thread() {}

  // Start the thread.
  bool Start() {
    int retval = pthread_create(&thread_, 0, Thread::StartRun,
                                static_cast<void*> (this));
    if (retval != 0)
      return false;
    running_ = true;
    return true;
  }
  // Wait for thread to exit.
  void Wait() {
    if (this->running_) {
      pthread_join(thread_, 0);
    }
  }

protected:
  // allow that the thread is canceled by another thread.
  void EnableCancel() {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
  }
  // forbid other threads to cancel this thread.
  void DisableCancel() {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
  }
  // the thread can only be canceled on certain points.
  void SetCancelDeferred() {
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
  }
  // the thread can be canceled at any time
  void SetCancelAsync() {
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
  }

  // thread's main function.
  virtual void Run() = 0;
  // cleanup, when thread is ended.
  virtual void Cleanup() {}

  // terminates the thread.
  void ExitThread() {
    if (this->running_)
      pthread_exit(0);
    running_ = false;
  }

  pthread_t ThreadId() const {
    return thread_;
  }

private:
  static void* StartRun(void *obj) {
    Thread *curObj = static_cast<Thread*> (obj);
    pthread_cleanup_push(Thread::StartCleanup, obj);
    curObj->Run();
    pthread_cleanup_pop(0);
    return 0;
  }
  static void StartCleanup(void *obj) {
    Thread *curObj = static_cast<Thread*> (obj);
    curObj->Cleanup();
  }
  bool running_;
  pthread_t thread_;
};

// A mutex.
// A mutex is a MUTual EXclusion device, and is useful for protecting
// shared data structures from concurrent modifications, and implementing
// critical sections and monitors
class Mutex {
private:
  // disable copying
  Mutex(const Mutex&);
  Mutex& operator=(const Mutex&);
public:
  Mutex() {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&mutex_, &attr);
    pthread_mutexattr_destroy(&attr);
  }
  explicit Mutex(const pthread_mutex_t &m) :
    mutex_(m) {
  }

  ~Mutex() {
    pthread_mutex_destroy(&mutex_);
  }

  bool Lock() {
    return (pthread_mutex_lock(&mutex_) == 0);
  }

  bool Unlock() {
    return (pthread_mutex_unlock(&mutex_) == 0);
  }

  bool Release() {
    return Unlock();
  }

  bool Trylock() {
    return (pthread_mutex_trylock(&mutex_) == 0);
  }

private:
  pthread_mutex_t mutex_;
  friend class Condition;
};

class MutexLock {
public:
  explicit MutexLock(Mutex *mutex) : mutex_(mutex) {
    mutex_->Lock();
  }
  ~MutexLock() {
    mutex_->Unlock();
  }
private:
  Mutex *mutex_;
};

// A thread-safety smart pointer.
// See A. Alexandrescu, "volatile - Multithreaded Programmer's Best Friend"
template<class T>
class LockingPointer {
public:
  LockingPointer(volatile T &obj, const volatile Mutex &mutex) :
    obj_(const_cast<T*>(&obj)), mutex_(const_cast<Mutex*>(&mutex)) {
    mutex_->Lock();
  }
  ~LockingPointer() {
    mutex_->Unlock();
  }
  T& operator*() {
    return *obj_;
  }
  T* operator->() {
    return obj_;
  }
  Mutex& GetMutex() {
    return *mutex_;
  }
private:
  T *obj_;
  Mutex *mutex_;
  DISALLOW_COPY_AND_ASSIGN(LockingPointer);
};

// read-write lock.
class ReadWriteLock {
private:
  ReadWriteLock(const ReadWriteLock&);
  ReadWriteLock& operator=(const ReadWriteLock&);
public:
  ReadWriteLock() {
    pthread_rwlock_init(&lock_, 0);
  }

  ~ReadWriteLock() {
    pthread_rwlock_destroy(&lock_);
  }

  // acquire a read lock.
  bool ReadLock() {
    return (pthread_rwlock_rdlock(&lock_) == 0);
  }
  // try to acquire a read lock.
  bool TryReadLock() {
    return (pthread_rwlock_tryrdlock(&lock_) == 0);
  }
  // acquire a write lock.
  bool WriteLock() {
    return (pthread_rwlock_wrlock(&lock_) == 0);
  }
  // try to acquire a write lock
  bool TryWriteLock() {
    return (pthread_rwlock_trywrlock(&lock_) == 0);
  }
  // unlock
  bool Unlock() {
    return (pthread_rwlock_unlock(&lock_) == 0);
  }
private:
  pthread_rwlock_t lock_;
};

// A condition variable.
// A condition variable is a synchronization device that allows
// threads to suspend execution and relinquish the processors
// until some predicate on shared data is satisfied.
class Condition {
private:
  Condition(const Condition&);
  Condition& operator=(const Condition&);
public:
  Condition() {
    pthread_cond_init(&cond_, 0);
  }

  ~Condition() {
    pthread_cond_destroy(&cond_);
  }


  // Restarts one of the threads that are waiting on the condition
  // variable.
  // @param broadcastSignal Restarts all waiting threads
  bool Signal(bool broadcast_signal = false) {
    if (broadcast_signal)
      return Broadcast();
    return (pthread_cond_signal(&cond_) == 0);
  }

  // Restart all waiting threads.
  // see Signal()
  bool Broadcast() {
    return (pthread_cond_broadcast(&cond_) == 0);
  }

  // Wait for the condition to be signaled.
  // The thread execution is suspended and does not consume any CPU time
  // until the condition variable is signaled.
  bool Wait() {
    mutex_.Lock();
    bool r = (pthread_cond_wait(&cond_, &mutex_.mutex_) == 0);
    mutex_.Unlock();
    return r;
  }

  // Wait for the condition to be signaled.
  // Instead of using the internal mutex, use the given one.
  bool Wait(Mutex &mutex) {
    bool r = (pthread_cond_wait(&cond_, &mutex.mutex_) == 0);
    return r;
  }

  // Wait for the condition to be signaled with a bounded wait time.
  // @see wait
  bool TimedWait(unsigned long microsonds) {
    mutex_.Lock();
    timespec timeout = GetTimeout(microsonds);
    bool r = (pthread_cond_timedwait(&cond_, &mutex_.mutex_, &timeout) == 0);
    mutex_.Unlock();
    return r;
  }

private:
  timespec GetTimeout(unsigned long microseconds) {
    timeval now;
    gettimeofday(&now, 0);
    timespec timeout;
    timeout.tv_sec = now.tv_sec + microseconds / 1000000;
    timeout.tv_nsec = now.tv_usec * 1000 + microseconds % 1000000;
    return timeout;
  }

  pthread_cond_t cond_;


  // A condition variable must always be associated with a mutex,
  // to avoid the race condition where a thread prepares to wait
  // on a condition variable and another thread signals the condition
  // just before the first thread actually waits on it.
  Mutex mutex_;
};

// =================================================================

template<class M>
class NullReducer {
public:
  void Reduce(M*) {}
};

template<class T>
class NullMapper {
public:
  NullMapper* Clone() const { return new NullMapper(); }
  void Map(const T &t) {}
  void Reset() {}
};

template<class Pool>
class ThreadPoolThread : public Thread {
public:
  typedef typename Pool::Mapper Mapper;
  ThreadPoolThread(Pool *pool, Mapper *mapper) : pool_(pool), mapper_(mapper) {}
  virtual ~ThreadPoolThread() {}
  void Run() {
    while (pool_->ExecuteTask(mapper_));
    pool_->ThreadTerminated(this);
  }
  Mapper* GetMapper() {
    return mapper_;
  }

private:
  Pool *pool_;
  Mapper *mapper_;
};

// The thread pool implementation is inspired and partly adapted from
// thread_pool in the Boost library.
template<class T, class M, class R>
class ThreadPoolImpl {
public:
  typedef T Task;
  typedef M Mapper;
  typedef R Reducer;
  typedef ThreadPoolImpl<T, M, R> Self;
  typedef ThreadPoolThread<Self> WorkerThread;


  ThreadPoolImpl() :
    active_threads_(0), running_threads_(0), terminate_(false) {}
  ~ThreadPoolImpl() {
    for (typename std::vector<WorkerThread*>::iterator t = threads_.begin();
        t != threads_.end(); ++t) {
      delete (*t)->GetMapper();
      delete *t;
    }
  }

  void Init(int num_threads, const Mapper &mapper) volatile {
    LockingPointer<Self> self(*this, monitor_);
    CHECK_EQ(self->threads_.size(), 0);
    active_threads_ = 0;
    for (int t = 0; t < num_threads; ++t) {
      self->threads_.push_back(new WorkerThread(const_cast<Self*>(this),
                                                mapper.Clone()));
      self->threads_.back()->GetMapper()->Reset();
      ++active_threads_;
      ++running_threads_;
      self->threads_.back()->Start();
    }
  }

  void Reset() volatile {
    Wait();
    LockingPointer<Self> self(*this, monitor_);
    for (int t = 0; t < self->threads_.size(); ++t) {
      self->threads_[t]->GetMapper()->Reset();
    }
  }

  void Submit(const Task &task) volatile {
    LockingPointer<Self> self(*this, monitor_);
    self->tasks_.push_back(task);
    self->new_task_.Signal();
  }

  void Wait() const volatile {
    // remove the volatile flag from this
    LockingPointer<const Self> self(*this, monitor_);
    while (self->active_threads_ > 0 || !self->tasks_.empty()) {
      self->thread_idle_.Wait(self.GetMutex());
    }
  }

  void Shutdown() {
    Wait();
    TerminateThreads();
  }

  // execute the given task.
  // called by the worker threads.
  bool ExecuteTask(Mapper *mapper) volatile {
    Task task;
    bool have_task = false;
    {
      LockingPointer<Self> self(*this, monitor_);
      if (self->terminate_)
        return false;

      while (self->tasks_.empty()) {
        if (self->terminate_) {
          return false;
        } else {
          --active_threads_;
          self->thread_idle_.Broadcast();
          self->new_task_.Wait(self.GetMutex());
          ++active_threads_;
        }
      }
      task = self->tasks_.front();
      self->tasks_.pop_front();
      have_task = true;
    }
    if (have_task) {
      mapper->Map(task);
    }
    return true;
  }

  void ThreadTerminated(WorkerThread *) volatile {
    LockingPointer<Self> self(*this, monitor_);
    --active_threads_;
    --running_threads_;
    self->thread_idle_.Broadcast();
  }

  void Combine(Reducer *reducer) {
    Wait();
    for (int t = 0; t < threads_.size(); ++t) {
      reducer->Reduce(threads_[t]->GetMapper());
    }
  }

  int NumRunningThreads() const volatile {
    return active_threads_;
  }

  int NumWaitingTasks() const volatile {
    LockingPointer<Self> self(*this, monitor_);
    return self->tasks_.size();
  }

  int Empty() const volatile {
    LockingPointer<Self> self(*this, monitor_);
    return self->tasks_.empty();
  }

private:
  void TerminateThreads() volatile {
    LockingPointer<Self> self(*this, monitor_);
    self->terminate_ = true;
    self->new_task_.Broadcast();
    while (running_threads_ > 0) {
      self->thread_idle_.Wait(self.GetMutex());
    }
    for (typename std::vector<WorkerThread*>::iterator t = self->threads_.begin();
        t != self->threads_.end(); ++t) {
      (*t)->Wait();
    }
  }

  std::vector<WorkerThread*> threads_;
  std::deque<Task> tasks_;
  // active threads is accessed by multiple threads
  volatile int active_threads_;
  volatile int running_threads_;
  bool terminate_;
  mutable Mutex monitor_;
  mutable Condition thread_idle_;
  mutable Condition new_task_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolImpl);
};

// A basic thread pool implementation offering a very simple shared memory
// Map-Reduce framework.
//
// The threads are created once and live as long as the pool lives.
//
// The idea is that a list of task gets processed by a number of Mapper
// objects and the results of the Mappers are afterwards combined with a
// Reducer object. The mapper should store the intermediate results.
// Each mapper runs in a separate thread. Only one reducer will be used.
//
// The class Task (T) should carry the input data. It has no required members.
// Task can either be a type or a pointer to the task type. In the latter case
// the Mapper or the Reducer has to delete the Task.
// Pointers should be chosen if copying a Task object is expensive, if the
// Task is modified by the Mapper, or if the Tasks are used in the Reducer.
//
// The class Mapper (M) must have the following methods:
// class Mapper {
//   // create a new Mapper object
//   Mapper* Clone() const;
//   // process task data
//   void Map(Task &t);
//   // clear all intermediate results.
//   // called with ThreadPool::Reset()
//   void Reset()
//  };
//
//  The class Reducer (R) must have the following method:
//  class Reducer {
//    // accumulate the results of a Mapper
//    void Reduce(Mapper *mapper);
//  };
//
template<class T, class M = NullMapper<T>, class R = NullReducer<M> >
class ThreadPool {
public:
  typedef ThreadPoolImpl<T, M, R> Impl;
  typedef typename Impl::Task Task;
  typedef typename Impl::Mapper Mapper;
  typedef typename Impl::Reducer Reducer;

  ThreadPool() : impl_(new Impl()) {}
  virtual ~ThreadPool() {
    impl_->Shutdown();
    delete impl_;
  }
  // initialize the pool with the given number of threads and copies of
  // of the given prototype Mapper.
  void Init(int num_threads, const Mapper &mapper = Mapper()) {
    impl_->Init(num_threads, mapper);
  }
  // reset all mappers.
  // waits for all tasks to be finished before resetting the mappers.
  void Reset() {
    impl_->Reset();
  }
  // submit a new task to be processed by a mapper.
  void Submit(const Task &task) {
    impl_->Submit(task);
  }
  // wait for all tasks to be completed.
  void Wait() {
    impl_->Wait();
  }
  // combine the results of the mappers using the given reducer.
  // waits for all tasks to be finished before applying the reducer.
  void Combine(Reducer *reducer) {
    impl_->Combine(reducer);
  }
protected:
  Impl *impl_;
  DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

}  // namespace threads

#endif  // THREAD_H_
