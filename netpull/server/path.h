/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

#include "netpull/assert.h"
#include "netpull/console.h"

namespace netpull {

namespace path_internal {

  std::string Join(std::string_view self, std::string_view other);
  bool IsAbsolute(std::string_view self);
  bool IsResolved(std::string_view self);
  bool IsChildOf(std::string_view self, std::string_view other);
  std::optional<std::string> Resolve(std::string_view self);

}

// A PathBase represents a wrapper over either a std::string or a std::string_view that
// provides path operations on top of it.
// Why not std::filesystem::path? PathBase is both lighter (due to the presence of
// PathView = PathBase<std::string_view>) and allows us to be stricter due to its
// platform-dependence.
template <typename T>
class PathBase {
  template <typename U>
  static constexpr bool is_valid_type_v = std::is_same_v<U, std::string> ||
                                          std::is_same_v<U, std::string_view>;

public:
  static_assert(is_valid_type_v<T>);

  template <typename U, typename V = U>
  using make_ref_or_copy_t = std::enable_if_t<
                                is_valid_type_v<V>,
                                // XXX: why doesn't add_const_t<add_lvalue_reference_t<U>> work
                                std::conditional_t<std::is_same_v<V, std::string>, const U&, U>>;

  using type = std::remove_reference_t<T>;

  PathBase() {}
  PathBase(type path): path_(path) {}

  // Convert between two different types of paths.
  template <typename U>
  PathBase(const PathBase<U>& other): path_(other.path()) {}

  // Returns the underlying path, either a const std::string& or a std::string_view.
  make_ref_or_copy_t<type> path() const { return path_; }

  // Join this path with another path or string.
  PathBase<std::string> operator/(PathBase<std::string_view> other) const {
    return path_internal::Join(path_, other.path());
  }

  PathBase<std::string> operator/(std::string_view other) const {
    return path_internal::Join(path_, other);
  }

  // Is this path absolute?
  bool IsAbsolute() const {
    return path_internal::IsAbsolute(path_);
  }

  // Is this path fully resolved? (No duplicate /'s, no '.' or '..').
  bool IsResolved() const {
    return path_internal::IsResolved(path_);
  }

  // Is this path a child of the other path? Both paths MUST be absolute and fully resolved.
  bool IsChildOf(PathBase<std::string_view> other) const {
    return path_internal::IsChildOf(path_, other.path());
  }

  // Returns this path relative to another path. Both paths MUST be absolute and fully
  // resolved.
  PathBase<T> RelativeTo(PathBase<std::string_view> other) const {
    NETPULL_ASSERT(IsChildOf(other), "%s is not a child of %s", path_, other.path());
    return path_.substr(other.path().size() + 1);
  }

  // Fully resolve the path, expanding any symlinks along the way as well.
  std::optional<PathBase<std::string>> Resolve() const {
    if (auto opt_path = path_internal::Resolve(path_)) {
      return *opt_path;
    } else {
      return {};
    }
  }

private:
  T path_;
};

// A PathBase over a string.
using Path = PathBase<std::string>;
// A PathBase over a std::string_view.
using PathView = PathBase<std::string_view>;

std::ostream& operator<<(std::ostream& os, Path path);
std::ostream& operator<<(std::ostream& os, PathView path);

}  // namespace netpull
