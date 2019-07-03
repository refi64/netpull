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

// A FastCrawler is designed to quickly crawl large directory trees. Subclasses should
// implement HandleObject, where they will be passed all filesystem objects found during a crawl.
class FastCrawler {
public:
  FastCrawler() {}

  // Represents an entity on the filesystem.
  class FsObject {
  public:
    enum class Type { kDirectory, kFile, kSymlink };

    Type type() const { return type_; }
    // The name of the object (the last component of the path).
    std::string_view name() const { return name_; }
    // The root path being visited that this object was found somewhere underneath.
    PathView root() const { return root_; }
    // The full path to the object.
    const Path& path() const { return path_; }
    // If this is a link, then this will return the link's target.
    const Path& link_target() const { return link_target_; }
    // Returns the inode ID.
    std::int64_t inode() const { return inode_; }
    // Returns the stat results of the filesystem object (not following symlinks).
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

  // Visits the given root path. This path will be the result of calling root() on any of
  // the resulting objects.
  void Visit(PathView root);

protected:
  // HandleObject should be implemented by FastCrawler subclasses.
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
