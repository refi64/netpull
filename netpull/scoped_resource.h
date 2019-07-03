/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <stdlib.h>
#include <unistd.h>

#include <functional>
#include <optional>

namespace netpull {

// A ScopedResource encapsulates some object / resource of type Type. Upon destruction, the
// ScopedResource will call an instance of Deleter on the object to release its resources.
template <typename Type, typename Deleter>
class ScopedResource {
public:
  ScopedResource() {}

  explicit ScopedResource(Type value): value_{std::move(value)} {}

  ScopedResource(const ScopedResource<Type, Deleter>& other)=delete;

  ScopedResource(ScopedResource<Type, Deleter>&& other) {
    if (!other.empty()) {
      value_ = std::move(*other.value_);
      other.value_.reset();
    }
  }

  ~ScopedResource() {
    reset();
  }

  // Is the resource empty?
  bool empty() const {
    return !value_;
  }

  // Release the contents of this resource and reset it.
  void reset() {
    if (!empty()) {
      deleter(**this);
    }

    value_.reset();
  }

  // Same as above, but places a new value in place of the old one.
  void reset(Type value) {
    reset();
    value_ = value;
  }

  // Access the resource.
  const Type& operator*() const {
    return *value_;
  }

  Type& operator*() {
    return *value_;
  }

  const Type* operator->() const {
    return &*value_;
  }

  Type* operator->() {
    return &*value_;
  }

  // Remove the resource from the ScopedResource's management.
  Type steal() {
    auto copy = *value_;
    value_.reset();
    return copy;
  }

private:
  void destroy();

  std::optional<Type> value_;
  Deleter deleter;
};

namespace scoped_resource_internal {
  struct FdDeleter {
    void operator()(int fd) { close(fd); }
  };

  struct DeferDeleter {
    void operator()(std::function<void()> func) { func(); }
  };
}

// A ScopedResource specialization for file descriptors.
using ScopedFd = ScopedResource<int, scoped_resource_internal::FdDeleter>;
// A ScopedResource specialization that runs an arbitrary function on destruction.
using ScopedDefer = ScopedResource<std::function<void()>,
                                   scoped_resource_internal::DeferDeleter>;

}  // namespace netpull
