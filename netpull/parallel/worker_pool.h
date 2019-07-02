/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "absl/synchronization/mutex.h"

#include <memory>
#include <queue>
#include <thread>
#include <vector>

namespace netpull {

class SubmissionKey {
public:
  SubmissionKey() {}
  SubmissionKey(const SubmissionKey& other)=delete;
  SubmissionKey(SubmissionKey&& other)=delete;

  void WaitForPending();

private:
  static bool StaticConditionCheck(SubmissionKey* self);

  void AddPending();
  void RemovePending();

  absl::Mutex mutex;
  int pending = 0;

  friend class Task;
};

class WorkerPool;

class Task {
public:
  Task(SubmissionKey* key): key_(key) {}
  virtual ~Task() {}

  virtual int priority() const { return 0; }
  virtual void Run(WorkerPool* pool)=0;

  const SubmissionKey& key() const { return *key_; }
  SubmissionKey* mutable_key() { return key_; }

private:
  void Prepare();
  void Execute(WorkerPool* pool);

  SubmissionKey* key_;

  friend class WorkerPool;
};

class WorkerPool {
public:
  WorkerPool(int worker_count): worker_count_(worker_count) {}
  WorkerPool(const WorkerPool& other)=delete;
  WorkerPool(WorkerPool&& other)=delete;
  ~WorkerPool() { WaitForCompletion(); }

  void Start();
  void Submit(std::unique_ptr<Task> task);
  void Done();
  void WaitForCompletion();

  int worker_count() const { return worker_count_; }

private:
  class ThreadSafeTaskQueue {
  public:
    ThreadSafeTaskQueue() {}

    std::unique_ptr<Task> WaitForTaskAndPop();
    void Submit(std::unique_ptr<Task> task);
    void Done();

  private:
  struct TaskIsLowerPriorityComparator {
    bool operator()(const std::unique_ptr<Task>& a, const std::unique_ptr<Task>& b) {
      return a->priority() < b->priority();
    }
  };

    static bool StaticConditionCheck(ThreadSafeTaskQueue* self);

    absl::Mutex mutex;
    std::priority_queue<std::unique_ptr<Task>, std::vector<std::unique_ptr<Task>>,
                        TaskIsLowerPriorityComparator> queue;
    bool done = false;
  };

  static void Worker(WorkerPool* pool);

  std::vector<std::thread> threads;
  ThreadSafeTaskQueue tasks;
  int worker_count_;
};

}  // namespace netpull
