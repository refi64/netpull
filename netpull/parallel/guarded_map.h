/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace netpull {

// A thread-safe wrapper over a flat_hash_map, with special members to better suit a parallel
// use case.
template <typename K, typename V>
class GuardedMap {
public:
  GuardedMap(): mutex(new absl::Mutex) {}
  GuardedMap(const GuardedMap<K, V>& other)=delete;
  GuardedMap(GuardedMap<K, V>&& other)=default;

  GuardedMap<K, V>& operator=(GuardedMap<K, V>&& other)=default;

  using key_type = K;
  using mapped_type = V;

  // Place the given key and value into the map, returning true if it was actually inserted,
  // or false if the insertion failed due to the key already being present.
  bool Put(const K& k, const V& v) {
    absl::MutexLock lock(mutex.get());
    return data.try_emplace(k, v).second;
  }

  bool Put(K&& k, V&& v) {
    absl::MutexLock lock(mutex.get());
    return data.try_emplace(k, v).second;
  }

  // Find the given key. If its value, when passed to decider, returns in true, then
  // remove it from the map, and return it. If the decider was false, or the key was not
  // present, return an empty optional.
  template <typename F, typename = std::enable_if_t<std::is_invocable_r_v<bool, F, const V&>>>
  std::optional<V> GetAndPopIf(const K& k, F decider) {
    absl::MutexLock lock(mutex.get());
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

  // Similar to GetAndPopIf, but the decider always returns true.
  std::optional<V> GetAndPop(const K& k) {
    GetAndPopIf(k, []() { return true; });
  }

  // Wait until the GuardedMap is empty.
  void WaitForEmpty() {
    mutex->LockWhen(absl::Condition(&GuardedMap<K, V>::StaticConditionCheck, this));
    mutex->Unlock();
  }

private:
  static bool StaticConditionCheck(GuardedMap<K, V>* self) {
    return self->data.empty();
  }

  std::unique_ptr<absl::Mutex> mutex;
  absl::flat_hash_map<K, V> data;
};

}  // namespace netpull
