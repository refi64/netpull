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

// A SubmissionKey is an identifier that can be passed to a task. A key is "pending" when there
// are tasks that use it that have not run yet. In essence, this allows us to wait for a subset
// of the tasks submitted to a pull.
class SubmissionKey {
public:
  SubmissionKey() {}
  SubmissionKey(const SubmissionKey& other)=delete;
  SubmissionKey(SubmissionKey&& other)=delete;

  // Wait for any tasks that have this key to complete.
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

// A Task is a runnable entity that can be given to a WorkerPool. A key may optionally passed
// to the task, meaning that a wait on the key will wait for the tasks holding it to complete
// their execution. (The key may be nullptr as well.)
class Task {
public:
  Task(SubmissionKey* key): key_(key) {}
  virtual ~Task() {}

  // Returns the task's priority. Higher values mean the task will be run first.
  virtual int priority() const { return 0; }
  // Runs the given task.
  virtual void Run(WorkerPool* pool)=0;

  const SubmissionKey& key() const { return *key_; }
  SubmissionKey* mutable_key() { return key_; }

private:
  void Prepare();
  void Execute(WorkerPool* pool);

  SubmissionKey* key_;

  friend class WorkerPool;
};

// A WorkerPool is a pool of worker threads that tasks can be passed to.
class WorkerPool {
public:
  WorkerPool(int worker_count): worker_count_(worker_count) {}
  WorkerPool(const WorkerPool& other)=delete;
  WorkerPool(WorkerPool&& other)=delete;
  ~WorkerPool() { WaitForCompletion(); }

  // Start the worker pool.
  void Start();
  // Submit a task to the worker pool. The task's key (if present) will immediately be marked as
  // "pending", and that will not be cleared until all tasks using the key are run.
  void Submit(std::unique_ptr<Task> task);
  // Mark the WorkerPool as done. If there are stil pending Tasks, they will be dropped.
  void Done();
  // Wait for all tasks to complete, and shut down the workers.
  void WaitForCompletion();

  // Returns the number of workers running.
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
