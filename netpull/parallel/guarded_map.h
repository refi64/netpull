/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace netpull {

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

}  // namespace netpull
