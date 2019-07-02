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
  GuardedSet() {}
  GuardedSet(const GuardedSet<T>& other)=delete;
  GuardedSet(GuardedSet<T>&& other)=default;

  using value_type = T;

  void Insert(const T& value) {
    absl::MutexLock lock(&mutex);
    data.insert(value);
  }

  void Insert(T&& value) {
    absl::MutexLock lock(&mutex);
    data.insert(value);
  }

  void Erase(const T& value) {
    data.erase(value);
  }

  absl::flat_hash_set<T> pull() {
    return std::move(data);
  }

private:
  absl::Mutex mutex;
  absl::flat_hash_set<T> data;
};

}  // namespace netpull
