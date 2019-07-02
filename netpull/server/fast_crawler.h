/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <cinttypes>
#include <queue>
#include <string>
#include <string_view>

#include <sys/stat.h>

#include "netpull/scoped_resource.h"

#include "netpull/server/path.h"

namespace netpull {

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

}  // namespace netpull
