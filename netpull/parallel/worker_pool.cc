/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "netpull/console.h"
#include "netpull/scoped_resource.h"

#include "worker_pool.h"

namespace netpull {

void SubmissionKey::WaitForPending() {
  mutex.LockWhen(absl::Condition(&SubmissionKey::StaticConditionCheck, this));
  mutex.Unlock();
}

bool SubmissionKey::StaticConditionCheck(SubmissionKey* self) {
  return self->pending == 0;
}

void SubmissionKey::AddPending() {
  absl::MutexLock lock(&mutex);
  pending++;
}

void SubmissionKey::RemovePending() {
  absl::MutexLock lock(&mutex);
  pending--;
}

void Task::Prepare() {
  if (key_) {
    key_->AddPending();
  }
}

void Task::Execute(WorkerPool* pool) {
  ScopedDefer key_resource([&]() {
    if (key_) {
      key_->RemovePending();
    }
  });
  Run(pool);
}

std::unique_ptr<Task> WorkerPool::ThreadSafeTaskQueue::WaitForTaskAndPop() {
  mutex.LockWhen(absl::Condition(&ThreadSafeTaskQueue::StaticConditionCheck, this));

  if (done) {
    mutex.Unlock();
    return {};
  }

  // TODO: figure out a better way to do this.
  std::unique_ptr<Task> task = std::move(const_cast<std::unique_ptr<Task>&>(queue.top()));
  queue.pop();

  mutex.Unlock();

  return task;
}

void WorkerPool::ThreadSafeTaskQueue::Submit(std::unique_ptr<Task> task) {
  absl::MutexLock lock(&mutex);
  queue.emplace(std::move(task));
}

void WorkerPool::ThreadSafeTaskQueue::Done() {
  absl::MutexLock lock(&mutex);
  done = true;
}

bool WorkerPool::ThreadSafeTaskQueue::StaticConditionCheck(ThreadSafeTaskQueue* self) {
  return !self->queue.empty() || self->done;
}

void WorkerPool::Start() {
  for (int i = 0; i < worker_count_; i++) {
    threads.emplace_back(&WorkerPool::Worker, this);
  }
}

void WorkerPool::Submit(std::unique_ptr<Task> task) {
  task->Prepare();
  tasks.Submit(std::move(task));
}

void WorkerPool::Done() {
  tasks.Done();
}

void WorkerPool::WaitForCompletion() {
  Done();

  for (auto& thread : threads) {
    thread.join();
  }

  threads.clear();
}

void WorkerPool::Worker(WorkerPool* pool) {
  for (;;) {
    auto task = pool->tasks.WaitForTaskAndPop();
    if (!task) {
      return;
    }

    task->Execute(pool);
  }
}

}  // namespace netpull
