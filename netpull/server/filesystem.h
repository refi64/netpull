/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <cinttypes>
#include <queue>
#include <string>
#include <string_view>
#include <type_traits>

#include <sys/stat.h>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"

#include "netpull/scoped_resource.h"

namespace netpull {

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
    std::string_view other_path = other.path();

    bool we_have_slash = absl::EndsWith(path_, "/");
    bool other_has_slash = absl::StartsWith(other_path, "/");

    std::string p(path_);

    if (we_have_slash && other_has_slash) {
      other_path.remove_prefix(1);
    } else if (!we_have_slash && !other_has_slash) {
      p.push_back('/');
    }

    p += other_path;
    return p;
  }

  bool IsAbsolute() {
    return absl::StartsWith(path_, "/");
  }

  bool IsResolved() {
    std::string_view to_split(path_);
    if (IsAbsolute()) {
      if (path_ == "/") {
        return true;
      }

      to_split.remove_prefix(1);
    }

    std::vector<std::string_view> parts = absl::StrSplit(to_split, '/');
    for (auto part : parts) {
      if (part.empty() || part == "." || part == "..") {
        return false;
      }
    }

    return true;
  }

  bool IsChildOf(PathBase<std::string_view> other) {
    assert(IsAbsolute());
    assert(IsResolved());
    assert(other.IsAbsolute());
    assert(other.IsResolved());
    return absl::StartsWith(path_, other.path());
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

class FastCrawler {
public:
  FastCrawler() {}

  class FsObject {
  public:
    enum class Type { kDirectory, kFile, kSymlink };

    Type type() const { return type_; }
    // The resolved path to the root directory being crawled.
    std::string_view name() const { return name_; }
    PathView root() const { return root_; }
    const Path& path() const { return path_; }
    const Path& link_target() const { return link_target_; }
    std::int64_t inode() const { return inode_; }
    const struct stat& stat() const { return stat_; }

  private:
    FsObject() {}

    Type type_;
    PathView root_;
    std::string_view name_;
    Path path_;
    Path link_target_;
    std::int64_t inode_;
    struct stat stat_;

    friend class FastCrawler;
  };

  FastCrawler(const FastCrawler& other)=delete;

  void Visit(PathView root);

protected:
  virtual void HandleObject(const FsObject& object)=0;

private:
  struct QueuedObjectCrawl {
    QueuedObjectCrawl(ScopedFd parentfd, Path parent, std::string name):
      parentfd(std::move(parentfd)), parent(std::move(parent)), name(std::move(name)) {}

    ScopedFd parentfd;
    Path parent;
    std::string name;
  };

  void PumpDirectoryEntries(PathView root, int rawparentfd, PathView parent,
                            std::string_view name);

  std::queue<QueuedObjectCrawl> queue;
};

}
