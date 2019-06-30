/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

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

class FileStreamTask : public Task {
public:
  FileStreamTask(SubmissionKey* key, proto::PullResponse::PullObject::FileTransferInfo transfer,
                 std::string path, ScopedFd fd, const IpLocation& server):
    Task(key), transfer(transfer), path(std::move(path)), fd(std::move(fd)), server(server) {}

  void Run() override {
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

    line->Update(absl::StrFormat("Starting transfer of %s", path));

    while (total_bytes < transfer.bytes()) {
      line->Update(absl::StrFormat("Splicing for %s, read %d bytes of %d", path, total_bytes, transfer.bytes()));

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
      }
    }

    fsync(*fd);

    // TODO: integrity
  }

private:
  proto::PullResponse::PullObject::FileTransferInfo transfer;
  std::string path;
  ScopedFd fd;
  const IpLocation& server;
};

void HandleTransfer(proto::PullResponse::PullObject pull, std::string_view dest,
                    const ScopedFd& destfd, const IpLocation& server, WorkerPool* pool,
                    SubmissionKey* key) {
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
    auto task = new FileStreamTask(key, pull.transfer(), std::string(path), std::move(fd),
                                   server);
    pool->Submit(std::unique_ptr<Task>(task));
  }
}

void ConsoleUpdateThread(std::unique_ptr<ConsoleLine> status_line,
                         const absl::Notification& notification) {
  status_line->Update("Doing stuff...");

  while (!notification.WaitForNotificationWithTimeout(absl::Milliseconds(100))) {
    ConsoleLine::DrawAll();
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

  absl::Notification console_update_notification;
  std::thread console_update_thread(ConsoleUpdateThread, ConsoleLine::Claim(),
                                    std::ref(console_update_notification));

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

      /* LogInfo("Pull: %s", pull.path()); */
      assert(pull.path()[0] != '/');
      HandleTransfer(std::move(pull), dest, destfd, server, &pool, &key);
    } else if (resp.has_started()) {
      LogInfo("Pull started, job ID: %s", resp.started().job());
    } else if (resp.has_finished()) {
      LogInfo("Shutting down...");
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
