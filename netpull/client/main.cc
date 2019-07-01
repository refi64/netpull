/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <wchar.h>

#include <algorithm>
#include <sstream>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

#include "netpull/console.h"
#include "netpull/network.h"
#include "netpull/parallel.h"
#include "netpull/scoped_resource.h"

#include "netpull/netpull.pb.h"

using namespace netpull;

struct Utf8WidthInformation {
  std::vector<size_t> char_indexes;
  std::vector<int> char_widths;
  size_t total_width = 0;

  static Utf8WidthInformation ForString(std::string_view str) {
    static_assert(sizeof(wchar_t) == 4);

    Utf8WidthInformation info;

    constexpr int
      kOneByteMask = 0x80,
      kOneByteValue = 0,
      kTwoByteMask = 0xE0,
      kTwoByteValue = 0xC0,
      kThreeByteMask = 0xF0,
      kThreeByteValue = kTwoByteMask,
      kFourByteMask = 0xF8,
      kFourByteValue = kThreeByteMask,
      kSequenceMask = 0x3F;

    wchar_t current_char = 0;
    for (auto it = str.begin(); it != str.end(); ) {
      char c = *it;
      info.char_indexes.push_back(it - str.begin());

      if ((c & kOneByteMask) == kOneByteValue) {
        current_char = *it++;
      } else if ((c & kTwoByteMask) == kTwoByteValue) {
        if (str.end() - it < 2) {
          continue;
        }

        current_char = (*it++ & ~kTwoByteMask) << 6;
        current_char |= *it++ & kSequenceMask;
      } else if ((c & kThreeByteMask) == kThreeByteValue) {
        if (str.end() - it < 3) {
          continue;
        }

        current_char = (*it++ & ~kThreeByteMask) << 12;
        current_char |= (*it++ & kSequenceMask) << 6;
        current_char |= *it++ & kSequenceMask;
      } else if ((c & kFourByteMask) == kFourByteValue) {
        if (str.end() - it < 4) {
          continue;
        }

        current_char = (*it++ & ~kFourByteMask) << 18;
        current_char |= (*it++ & kSequenceMask) << 12;
        current_char |= (*it++ & kSequenceMask) << 6;
        current_char |= *it++ & kSequenceMask;
      }

      int width = wcwidth(current_char);
      info.char_widths.push_back(width);
      info.total_width += width;
    }

    return info;
  }
};

class ProgressState {
public:
  enum class Action {
    kDownload,
    kVerify,
  };

  ProgressState(Action action, std::string_view item):
    action(action), item(item), item_utf8(Utf8WidthInformation::ForString(item)) {}

  std::string BuildLine(double progress) {
    struct winsize ws;
    int columns = 75;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1) {
      LogErrno("ioctl(1, TIOCGWINSZ)");
    } else {
      columns = ws.ws_col;
    }

    // Leave an extra space.
    columns--;

    // (iostreams aren't the fastest here and not too useful, might as well build it ourselves.)
    // XXX: trying to get a decent-length buffer size, assume any char may be full-width UTF-8.
    std::string result(std::max(columns * 4, 15), ' ');
    auto it = result.begin();

    std::string_view action_string;
    switch (action) {
    case Action::kDownload:
      action_string = kDownloadActionString;
      break;
    case Action::kVerify:
      action_string = kVerifyActionString;
      break;
    }

    it = std::copy(action_string.begin(), action_string.end(), it);
    it++;

    // 1/3 the screen width for the item, at most.
    int item_space = columns / 3;
    if (item_utf8.total_width > item_space) {
      std::string_view ellipses = "...";
      it = std::copy(ellipses.begin(), ellipses.end(), it);

      // Figure out how many UTF-8 chars to print.
      int current_width = 0;
      auto width_it = item_utf8.char_widths.rbegin();
      for (; width_it != item_utf8.char_widths.rend(); width_it++) {
        if (current_width + *width_it > item_space - ellipses.size()) {
          break;
        }

        current_width += *width_it;
      }

      // Find the byte character index.
      size_t index = item_utf8.char_indexes[item_utf8.char_widths.rend() - width_it];

      it = std::copy(item.begin() + index, item.end(), it);
    } else {
      it = std::copy(item.begin(), item.end(), it);
      it += item_space - item_utf8.total_width;
    }

    it++;
    *it++ = '[';

    // Action character + space + item + space + opening bracket.
    int cols_taken = 1 + 1 + item_space + 1 + 1;
    // Closing bracket + space + max percent size (XXX.X%).
    constexpr int kExtraLength = 1 + 1 + 6;

    int remaining_cols = columns - cols_taken - kExtraLength;
    int filled_cols = remaining_cols * progress;

    for (int i = 0; i < remaining_cols; i++) {
      *it++ = i < filled_cols ? '=' : '-';
    }

    *it++ = ']';
    *it++ = ' ';

    int progress_factor = 1000;
    int progress_scaled = progress * progress_factor;
    for (int i = 0; i < 4; i++) {
      if (i == 3) {
        *it++ = '.';
      }

      int digit = progress_scaled / progress_factor;
      if (digit == 0 && i < 2) {
        *it++ = ' ';
      } else {
        *it++ = digit + '0';
      }
      progress_scaled -= digit * progress_factor;
      progress_factor /= 10;
    }

    *it++ = '%';

    result.resize(it - result.begin());
    return result;
  }

private:
  constexpr static const char kDownloadActionString[] = "↓";
  constexpr static const char kVerifyActionString[] = "✓";

  Action action;
  std::string_view item;
  const Utf8WidthInformation item_utf8;
};

class FileStreamTask : public Task {
public:
  FileStreamTask(SubmissionKey* key, std::atomic<int64_t>* total_bytes_transferred,
                 std::atomic<int64_t>* total_items_transferred,
                 proto::PullResponse::PullObject::FileTransferInfo transfer,
                 const ScopedFd& destfd, std::string path, const IpLocation& server):
    Task(key), total_bytes_transferred(total_bytes_transferred),
    total_items_transferred(total_items_transferred), transfer(transfer),
    destfd(destfd), path(std::move(path)), server(server) {}

  void Run() override {
    ScopedFd fd;
    if (int rawfd = openat(*destfd, path.data(), O_WRONLY); rawfd != -1) {
      fd.reset(rawfd);
    } else {
      LogErrno("Failed to open %s", path);
      return;
    }

    std::array<int, 2> pipefd;
    if (pipe(pipefd.data()) == -1) {
      LogErrno("pipe");
      return;
    }

    ScopedFd read_end(pipefd[0]), write_end(pipefd[1]);

    auto conn = SocketConnection::Connect(server);
    if (!conn) {
      return;
    }

    proto::PullRequest req;
    proto::PullRequest::PullFile* file = req.mutable_file();
    file->set_id(transfer.id());
    LogVerbose("Requesting %s", transfer.id());

    if (!conn->SendProtobufMessage(req)) {
      return;
    }

    auto line = ConsoleLine::Claim();

    constexpr size_t kSpliceBuffer = 16 * 1024;

    uint64_t total_bytes = 0;

    ProgressState progress(ProgressState::Action::kDownload, path);

    LogVerbose("Starting transfer of %s", path);

    while (total_bytes < transfer.bytes()) {
      line->Update(progress.BuildLine(static_cast<double>(total_bytes) / transfer.bytes()));

      ssize_t expected_bytes = splice(*conn->fd(), nullptr, *write_end, nullptr, kSpliceBuffer,
                                      SPLICE_F_MOVE | SPLICE_F_MORE);
      if (expected_bytes == -1) {
        LogErrno("splice to pipe for %s", path);
        return;
      } else if (expected_bytes == 0) {
        LogError("Server ended pull for %s", path);
        return;
      }

      total_bytes += expected_bytes;

      while (expected_bytes > 0) {
        ssize_t sent_bytes = splice(*read_end, nullptr, *fd, nullptr, expected_bytes,
                                    SPLICE_F_MOVE | SPLICE_F_MORE);
        if (sent_bytes == -1) {
          LogErrno("splice to file for %s", path);
          return;
        }

        expected_bytes -= sent_bytes;
        *total_bytes_transferred += sent_bytes;
      }
    }

    fsync(*fd);
    (*total_items_transferred)++;

    // TODO: integrity
  }

private:
  std::atomic<int64_t>* total_bytes_transferred;
  std::atomic<int64_t>* total_items_transferred;
  proto::PullResponse::PullObject::FileTransferInfo transfer;
  const ScopedFd& destfd;
  std::string path;
  const IpLocation& server;
};

void HandleTransfer(proto::PullResponse::PullObject pull, std::string_view dest,
                    const ScopedFd& destfd, const IpLocation& server, WorkerPool* pool,
                    SubmissionKey* key, std::atomic<int64_t>* total_bytes_transferred,
                    std::atomic<int64_t>* total_items_transferred) {
  int open_flags = 0;
  mode_t open_mode = 0;

  std::string_view path(pull.path());

  switch (pull.type()) {
  case proto::PullResponse::PullObject::kTypeFile:
    open_flags = O_CREAT | O_RDWR;
    open_mode = pull.nonlink().perms();
    break;

  case proto::PullResponse::PullObject::kTypeDirectory:
    open_flags = O_DIRECTORY | O_RDONLY;

    {
      if (mkdirat(*destfd, path.data(), pull.nonlink().perms()) == -1 && errno != EEXIST) {
        LogErrno("Failed to create directory %s/%s", dest, path);
        return;
      }
    }

    break;

  case proto::PullResponse::PullObject::kTypeSymlink:
    open_flags = O_NOFOLLOW | O_RDONLY;

    {
      proto::PullResponse::PullObject::SymlinkData symlink = pull.symlink();
      std::string target;

      if (symlink.relative_to_root()) {
        target = dest;
        target += '/';
        target += symlink.target();
      } else {
        target = symlink.target();
      }

      if (symlinkat(target.data(), *destfd, path.data()) == -1 && errno != EEXIST) {
        LogErrno("Failed to create symlink %s/%s", dest, path);
        return;
      }
    }

    break;

  default:
    assert(false);
  }

  ScopedFd fd;
  if (int rawfd = openat(*destfd, path.data(), open_flags, open_mode); rawfd != -1) {
    fd.reset(rawfd);
  } else {
    LogErrno("Failed to open %s/%s", dest, path);
    return;
  }

  proto::PullResponse::PullObject::Ownership owner = pull.owner();

  uid_t uid = owner.uid();
  gid_t gid = owner.gid();

  if (!owner.user().empty()) {
    if (struct passwd* pwd = getpwnam(owner.user().data())) {
      uid = pwd->pw_uid;
    }
  }

  if (!owner.group().empty()) {
    if (struct group* gr = getgrnam(owner.group().data())) {
      gid = gr->gr_gid;
    }
  }

  if (fchown(*fd, uid, gid) == -1) {
    LogErrno("Failed to chown %s/%s", dest, path);
  }

  // TODO: times

  if (pull.has_transfer()) {
    auto task = new FileStreamTask(key, total_bytes_transferred, total_items_transferred,
                                   pull.transfer(), destfd, std::string(path), server);
    pool->Submit(std::unique_ptr<Task>(task));
  } else {
    (*total_items_transferred)++;
  }
}

void ConsoleUpdateThread(std::unique_ptr<ConsoleLine> status_line,
                         const absl::Notification& notification,
                         std::atomic<int64_t>* total_bytes_transferred,
                         const std::atomic<int64_t>& total_items_transferred,
                         const std::atomic<int64_t>& total_items) {
  time_t last_timestamp = 0;
  double mbps = 0;
  int transferred_this_second = 0;

  while (!notification.WaitForNotificationWithTimeout(absl::Milliseconds(100))) {
    if (time(nullptr) > last_timestamp) {
      mbps = transferred_this_second / 1000.0 / 1000.0;
      transferred_this_second = 0;
      last_timestamp = time(nullptr);
    }

    std::ostringstream ss;
    ss << "[" << total_items_transferred.load() << "/" << total_items.load() << "] transferred"
       << " at " << std::setprecision(2) << std::fixed << mbps << " MBps";
    status_line->Update(ss.str());
    ConsoleLine::DrawAll();

    transferred_this_second += total_bytes_transferred->exchange(0);
  }

  status_line->Update("Done!");
}

ABSL_FLAG(bool, verbose, false, "Be verbose");
ABSL_FLAG(IpLocation, server, IpLocation({127, 0, 0, 1}), "The server to connect to");
ABSL_FLAG(int, workers, 4, "The default number of file forwarding workers to use");

int main(int argc, char** argv) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  signal(SIGPIPE, SIG_IGN);

  std::vector<char*> c_args = absl::ParseCommandLine(argc, argv);
  if (c_args.size() != 3) {
    LogError("A directory to pull and target are required.");
    return 1;
  }

  if (absl::GetFlag(FLAGS_verbose)) {
    EnableVerboseLogging();
  }

  std::string_view directory(c_args[1]), dest(c_args[2]);

  if (mkdir(dest.data(), 0755) == -1 && errno != EEXIST) {
    LogErrno("mkdir of %s failed", dest);
  }

  ScopedFd destfd;
  if (int rawdestfd = open(dest.data(), O_DIRECTORY | O_RDONLY); rawdestfd != -1) {
    destfd.reset(rawdestfd);
  } else {
    LogErrno("Failed to open %s", dest);
  }

  IpLocation server = absl::GetFlag(FLAGS_server);
  auto conn = SocketConnection::Connect(server);
  if (!conn) {
    return 1;
  }

  std::atomic<int64_t> total_bytes_transferred(0);
  std::atomic<int64_t> total_items_transferred(0);
  std::atomic<int64_t> total_items(0);

  absl::Notification console_update_notification;
  std::thread console_update_thread(ConsoleUpdateThread, ConsoleLine::Claim(),
                                    std::ref(console_update_notification),
                                    &total_bytes_transferred, std::ref(total_items_transferred),
                                    std::ref(total_items));

  SubmissionKey key;

  WorkerPool pool(absl::GetFlag(FLAGS_workers));
  pool.Start();

  proto::PullRequest req;
  proto::PullRequest::PullStart* start = req.mutable_start();
  start->set_path(std::string(directory));

  if (!conn->SendProtobufMessage(req)) {
    return 1;
  }

  for (;;) {
    proto::PullResponse resp;
    if (!conn->ReadProtobufMessage(&resp)) {
      return 1;
    }

    if (resp.has_object()) {
      proto::PullResponse::PullObject pull = resp.object();
      total_items.store(std::max(total_items.load(), pull.number()));

      assert(pull.path()[0] != '/');
      HandleTransfer(std::move(pull), dest, destfd, server, &pool, &key,
                     &total_bytes_transferred, &total_items_transferred);
    } else if (resp.has_started()) {
      LogInfo("Pull started, job ID: %s", resp.started().job());
    } else if (resp.has_finished()) {
      pool.Done();
      pool.WaitForCompletion();

      console_update_notification.Notify();
      console_update_thread.join();
      return 0;
    } else if (resp.has_error()) {
      LogError("Server returned error: %s", resp.error().message());
    } else {
      LogError("Unexpected pull response from server");
      abort();
    }
  }

  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}
