/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <type_traits>

namespace netpull {

namespace path_internal {

  std::string Join(std::string_view self, std::string_view other);
  bool IsAbsolute(std::string_view self);
  bool IsResolved(std::string_view self);
  bool IsChildOf(std::string_view self, std::string_view other);

}

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

  template <typename U>
  PathBase(const PathBase<U>& other): path_(other.path()) {}

  make_ref_or_copy_t<type> path() const { return path_; }

  PathBase<std::string> operator/(PathBase<std::string_view> other) const {
    return path_internal::Join(path_, other.path());
  }

  bool IsAbsolute() {
    return path_internal::IsAbsolute(path_);
  }

  bool IsResolved() {
    return path_internal::IsResolved(path_);
  }

  bool IsChildOf(PathBase<std::string_view> other) {
    return path_internal::IsChildOf(path_, other.path());
  }

  PathBase<T> RelativeTo(PathBase<std::string_view> other) {
    assert(IsChildOf(other));
    return path_.substr(other.path().size() + 1);
  }

private:
  T path_;
};

using Path = PathBase<std::string>;
using PathView = PathBase<std::string_view>;

std::ostream& operator<<(std::ostream& os, Path path);
std::ostream& operator<<(std::ostream& os, PathView path);

}  // namespace netpull
