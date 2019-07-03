/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"

#include "netpull/console.h"
#include "netpull/crypto.h"
#include "netpull/netpull.pb.h"
#include "netpull/network/ip.h"
#include "netpull/network/socket.h"
#include "netpull/parallel/guarded_set.h"
#include "netpull/parallel/worker_pool.h"
#include "netpull/scoped_resource.h"

#include "netpull/client/progress_builder.h"

using namespace netpull;

class FileIntegrityTask : public Task {
public:
  FileIntegrityTask(SubmissionKey* key, std::string_view& job,
                    GuardedSet<std::string>* to_complete, const std::string& expected_digest,
                    const ScopedFd& destfd, std::string path, const IpLocation& server):
    Task(key), job(job), to_complete(to_complete), expected_digest(expected_digest),
    destfd(destfd), path(path), server(server) {}

  void Run(WorkerPool* pool) override {
    ScopedFd fd;
    if (int rawfd = openat(*destfd, path.data(), O_RDONLY); rawfd != -1) {
      fd.reset(rawfd);
    } else {
      LogErrno("Failed to open %s", path);
      return;
    }

    LogVerbose("Checking integrity of %s", path);

    std::array<std::byte, Sha256Builder::kRecommendedBufferSize> buffer;
    Sha256Builder builder;

    auto line = ConsoleLine::Claim();
    constexpr std::string_view kVerifyAction = "✓";
    ProgressBuilder progress(kVerifyAction, path);

    size_t total_bytes_processed = 0;
    size_t total_bytes = lseek(*fd, 0, SEEK_END);
    lseek(*fd, 0, SEEK_SET);

    for (;;) {
      line->Update(progress.BuildLine(static_cast<double>(total_bytes_processed) / total_bytes));

      ssize_t bytes_read = read(*fd, static_cast<void*>(buffer.data()), buffer.size());
      if (bytes_read == -1) {
        LogErrno("Failed to read from %s", path);
        return;
      } else if (bytes_read == 0) {
        break;
      }

      builder.Update(buffer, static_cast<size_t>(bytes_read));
      total_bytes_processed += bytes_read;
    }

    if (auto digest = builder.Finish()) {
      if (*digest != expected_digest) {
        LogError("Wrong digest %s", *digest);
      } else {
        to_complete->Erase(path);
      }
    }

    auto conn = SocketConnection::Connect(server);
    if (!conn) {
      return;
    }

    proto::PullRequest req;
    proto::PullRequest::PullObjectSuccess* success = req.mutable_success();
    success->set_job(job.data());
    success->set_path(path);
    conn->SendProtobufMessage(req);
  }

private:
  std::string_view job;
  GuardedSet<std::string>* to_complete;
  std::string expected_digest;
  const ScopedFd& destfd;
  std::string path;
  const IpLocation& server;
};

void CopyProtoTimestampToTimespec(struct timespec* out, const google::protobuf::Timestamp& ts) {
  out->tv_sec = ts.seconds();
  out->tv_nsec = ts.nanos();
}

bool SetTimestamps(const ScopedFd& destfd, std::string_view path,
                   const proto::PullResponse::PullObject::Times& proto_times) {
  std::array<struct timespec, 2> times;
  CopyProtoTimestampToTimespec(&times[0], proto_times.access());
  CopyProtoTimestampToTimespec(&times[1], proto_times.modify());

  if (utimensat(*destfd, path.data(), times.data(), AT_SYMLINK_NOFOLLOW) == -1) {
    LogError("Failed to set timestamps of %s", path);
    return false;
  }

  return true;
}

class FileStreamTask : public Task {
public:
  FileStreamTask(SubmissionKey* key, std::string_view job, GuardedSet<std::string>* to_complete,
                 std::atomic<int64_t>* total_bytes_transferred,
                 std::atomic<int64_t>* total_items_transferred,
                 proto::PullResponse::PullObject::FileTransferInfo transfer,
                 proto::PullResponse::PullObject::Times times,
                 const ScopedFd& destfd, std::string path, const IpLocation& server):
    Task(key), job(job), to_complete(to_complete),
    total_bytes_transferred(total_bytes_transferred),
    total_items_transferred(total_items_transferred), transfer(transfer), times(times),
    destfd(destfd), path(std::move(path)), server(server) {}

  void Run(WorkerPool* pool) override {
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

    constexpr size_t kSpliceBuffer = 16 * 1024;

    uint64_t total_bytes = 0;

    auto line = ConsoleLine::Claim();
    constexpr std::string_view kDownloadAction = "↓";
    ProgressBuilder progress(kDownloadAction, path);

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

    if (!SetTimestamps(destfd, path, times)) {
      return;
    }

    auto task = new FileIntegrityTask(mutable_key(), job, to_complete, transfer.sha256(),
                                      destfd, path, server);
    pool->Submit(std::unique_ptr<Task>(task));
  }

private:
  std::string_view job;
  GuardedSet<std::string>* to_complete;
  std::atomic<int64_t>* total_bytes_transferred;
  std::atomic<int64_t>* total_items_transferred;
  proto::PullResponse::PullObject::FileTransferInfo transfer;
  proto::PullResponse::PullObject::Times times;
  const ScopedFd& destfd;
  std::string path;
  const IpLocation& server;
};

void HandleTransfer(proto::PullResponse::PullObject pull, std::string_view dest,
                    const ScopedFd& destfd, const IpLocation& server, WorkerPool* pool,
                    SubmissionKey* key, std::string_view job,
                    GuardedSet<std::string>* to_complete,
                    std::atomic<int64_t>* total_bytes_transferred,
                    std::atomic<int64_t>* total_items_transferred) {
  int open_flags = 0;
  mode_t open_mode = 0;

  std::string path(pull.path());
  to_complete->Insert(path);

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
    auto task = new FileStreamTask(key, job, to_complete, total_bytes_transferred,
                                   total_items_transferred, pull.transfer(), pull.times(),
                                   destfd, std::string(path), server);
    pool->Submit(std::unique_ptr<Task>(task));
  } else {
    (*total_items_transferred)++;
    if (!SetTimestamps(destfd, path, pull.times())) {
      return;
    }

    to_complete->Erase(path);
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

  std::string_view path(c_args[1]), dest(c_args[2]);

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
  GuardedSet<std::string> to_complete;

  WorkerPool pool(absl::GetFlag(FLAGS_workers));
  pool.Start();

  proto::PullRequest req;
  if (absl::StartsWith(path, "@")) {
    path.remove_prefix(1);
    proto::PullRequest::PullContinue* continue_ = req.mutable_continue_();
    continue_->set_job(path.data());
  } else {
    proto::PullRequest::PullStart* start = req.mutable_start();
    start->set_path(std::string(path));
  }

  if (!conn->SendProtobufMessage(req)) {
    return 1;
  }

  std::string job_id;

  for (;;) {
    proto::PullResponse resp;
    if (!conn->ReadProtobufMessage(&resp)) {
      return 1;
    }

    if (resp.has_object()) {
      proto::PullResponse::PullObject pull = resp.object();
      total_items.store(std::max(total_items.load(), pull.number()));

      assert(!job_id.empty());
      assert(pull.path()[0] != '/');
      HandleTransfer(std::move(pull), dest, destfd, server, &pool, &key, job_id, &to_complete,
                     &total_bytes_transferred, &total_items_transferred);
    } else if (resp.has_started()) {
      job_id = resp.started().job();
      LogInfo("Pull started, job ID: %s", job_id);
    } else if (resp.has_finished()) {
      key.WaitForPending();
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
