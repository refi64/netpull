/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

#include <memory>
#include <queue>
#include <thread>
#include <vector>

namespace netpull {

template <typename T>
class GuardedSet {
public:
  GuardedSet() {}
  GuardedSet(const GuardedSet<T>& other)=delete;
  GuardedSet(GuardedSet<T>&& other)=default;

  using value_type = T;

  void Add(const T& value) {
    absl::MutexLock lock(&mutex);
    data.insert(value);
  }

  void Add(T&& value) {
    absl::MutexLock lock(&mutex);
    data.insert(value);
  }

  void Remove(const T& value) {
    data.erase(value);
  }

  absl::flat_hash_set<T> Pull() {
    return std::move(data);
  }

private:
  absl::Mutex mutex;
  absl::flat_hash_set<T> data;
};

template <typename K, typename V>
class GuardedMap {
public:
  GuardedMap() {}
  GuardedMap(const GuardedMap<K, V>& other)=delete;
  GuardedMap(GuardedMap<K, V>&& other)=default;

  using key_type = K;
  using mapped_type = V;

  bool Put(const K& k, const V& v) {
    absl::MutexLock lock(&mutex);
    return data.try_emplace(k, v).second;
  }

  bool Put(K&& k, V&& v) {
    absl::MutexLock lock(&mutex);
    return data.try_emplace(k, v).second;
  }

  std::optional<V> GetAndPop(const K& k) {
    GetAndPopIf(k, []() { return true; });
  }

  template <typename F, typename = std::enable_if_t<std::is_invocable_r_v<bool, F, const V&>>>
  std::optional<V> GetAndPopIf(const K& k, F decider) {
    absl::MutexLock lock(&mutex);
    auto it = data.find(k);
    if (it != data.end()) {
      if (decider(it->second)) {
        V moved_value = std::move(it->second);
        data.erase(it);
        return moved_value;
      }
    }

    return {};
  }

  void WaitForEmpty() {
    mutex.LockWhen(absl::Condition(&GuardedMap<K, V>::StaticConditionCheck, this));
    mutex.Unlock();
  }

private:
  static bool StaticConditionCheck(GuardedMap<K, V>* self) {
    return self->data.empty();
  }

  absl::Mutex mutex;
  absl::flat_hash_map<K, V> data;
};

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

}
