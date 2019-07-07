/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <dirent.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/syscall.h>

#include <array>

#include "netpull/assert.h"
#include "netpull/console.h"

#include "fast_crawler.h"

using namespace std::literals::string_view_literals;

struct linux_dirent64 {
  std::uint64_t  d_ino;
  std::int64_t   d_off;
  unsigned short d_reclen;
  unsigned char  d_type;
  char           d_name[];
};

static int getdents64(int rawfd, struct linux_dirent64* dp, int size) {
  return syscall(SYS_getdents64, rawfd, dp, size);
}

namespace netpull {

static std::optional<std::string> FindUnresolvedLinkTargetAt(int rawparentfd,
                                                             std::string_view child) {
  std::array<char, PATH_MAX + 1> buffer;

  ssize_t len = readlinkat(rawparentfd, child.data(), buffer.data(), buffer.size());
  if (len == -1) {
    LogErrno("readlinkat of %s", child);
    return {};
  }

  buffer[len] = '\0';
  return std::string(buffer.data());
}

void FastCrawler::Visit(PathView root) {
  NETPULL_ASSERT(root.IsResolved(), "Trying to visit unresolved root %s",
                 absl::FormatStreamed(root));

  PumpDirectoryEntries(root, AT_FDCWD, PathView(""), root.path());

  while (!queue.empty()) {
    auto& item = queue.front();
    PumpDirectoryEntries(root, *item.parentfd, item.parent, item.name);
    queue.pop();
  }
}

static std::string_view DirentTypeToString(int type) {
  switch (type) {
  case DT_REG: return "regular file";
  case DT_DIR: return "directory";
  case DT_CHR: return "character device";
  case DT_BLK: return "block device";
  case DT_FIFO: return "named pipe";
  case DT_LNK: return "symbolic link";
  case DT_SOCK: return "socket";
  default: return "unknown";
  }
}

void FastCrawler::PumpDirectoryEntries(PathView root, int rawparentfd, PathView parent,
                                       std::string_view name) {
  ScopedFd fd;
  if (int rawfd = openat(rawparentfd, name.data(), O_DIRECTORY | O_RDONLY); rawfd != -1) {
    fd.reset(rawfd);
  } else {
    LogErrno("Failed to open %s/%s", absl::FormatStreamed(parent), name);
    return;
  }

  auto full_path = parent / name;

  constexpr int kBufferSize = 2048;
  std::array<std::byte, kBufferSize> buffer;

  for (;;) {
    int bytes_read = getdents64(*fd, reinterpret_cast<linux_dirent64*>(buffer.data()),
                                buffer.size());
    if (bytes_read <= 0) {
      if (bytes_read == -1) {
        LogErrno("Failed to read children of %s", name);
      }
      return;
    }

    std::byte* ptr = buffer.data();
    while (ptr < buffer.data() + bytes_read) {
      auto dp = reinterpret_cast<linux_dirent64*>(ptr);
      ptr += dp->d_reclen;

      if (dp->d_name == "."sv || dp->d_name == ".."sv) {
        continue;
      }

      FsObject object;
      object.root_ = root;
      object.name_ = dp->d_name;
      object.path_ = full_path / std::string_view(dp->d_name);
      object.inode_ = dp->d_ino;

      if (fstatat(*fd, dp->d_name, &object.stat_, AT_SYMLINK_NOFOLLOW) == -1) {
        LogErrno("Failed to stat %s/%s", absl::FormatStreamed(full_path), dp->d_name);
        continue;
      }

      if (dp->d_type == DT_UNKNOWN) {
        if (S_ISREG(object.stat_.st_mode)) {
          dp->d_type = DT_REG;
        } else if (S_ISDIR(object.stat_.st_mode)) {
          dp->d_type = DT_DIR;
        } else if (S_ISCHR(object.stat_.st_mode)) {
          dp->d_type = DT_CHR;
        } else if (S_ISBLK(object.stat_.st_mode)) {
          dp->d_type = DT_BLK;
        } else if (S_ISFIFO(object.stat_.st_mode)) {
          dp->d_type = DT_FIFO;
        } else if (S_ISLNK(object.stat_.st_mode)) {
          dp->d_type = DT_LNK;
        } else if (S_ISSOCK(object.stat_.st_mode)) {
          dp->d_type = DT_SOCK;
        }
      }

      switch (dp->d_type) {
      case DT_REG:
        object.type_ = FsObject::Type::kFile;
        break;
      case DT_DIR:
        object.type_ = FsObject::Type::kDirectory;
        break;
      case DT_LNK:
        object.type_ = FsObject::Type::kSymlink;

        if (auto opt_target = FindUnresolvedLinkTargetAt(*fd, dp->d_name)) {
          object.link_target_ = *opt_target;
        }

        break;
      default:
        LogWarning("Ignoring unexpected type %s for %s/%s", DirentTypeToString(dp->d_type),
                   absl::FormatStreamed(full_path), dp->d_name);
        continue;
      }

      HandleObject(object);

      if (dp->d_type == DT_DIR) {
        if (int rawnewfd = dup(*fd); rawnewfd != -1) {
          queue.emplace(ScopedFd(rawnewfd), full_path, dp->d_name);
        } else {
          LogErrno("Failed to dup parent fd");
        }
      }
    }
  }
}

}  // namespace netpull
