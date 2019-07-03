/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

namespace netpull {

template <typename T>
class GuardedSet {
public:
  GuardedSet(): mutex(new absl::Mutex) {}
  GuardedSet(const GuardedSet<T>& other)=delete;
  GuardedSet(GuardedSet<T>&& other)=default;

  GuardedSet<T>& operator=(GuardedSet<T>&& other)=default;

  using value_type = T;

  void Insert(const T& value) {
    absl::MutexLock lock(mutex.get());
    data.insert(value);
  }

  void Insert(T&& value) {
    absl::MutexLock lock(mutex.get());
    data.insert(value);
  }

  bool Contains(const T& value) const {
    absl::MutexLock lock(mutex.get());
    return data.contains(value);
  }

  void Erase(const T& value) {
    data.erase(value);
  }

  absl::flat_hash_set<T> Pull() {
    return std::move(data);
  }

private:
  mutable std::unique_ptr<absl::Mutex> mutex;
  absl::flat_hash_set<T> data;
};

}  // namespace netpull
