/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <arpa/inet.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

#include <array>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/match.h"
#include "absl/time/time.h"

#include "netpull/console.h"
#include "netpull/crypto.h"
#include "netpull/network/ip.h"
#include "netpull/network/socket.h"
#include "netpull/parallel/guarded_map.h"
#include "netpull/parallel/guarded_set.h"
#include "netpull/parallel/worker_pool.h"
#include "netpull/scoped_resource.h"

#include "netpull/netpull.pb.h"

#include "netpull/server/fast_crawler.h"
#include "netpull/server/path.h"

using namespace netpull;

class Environment {
public:
  static Environment Create() {
    if (const char* xdg_cache_home_cstr = getenv("XDG_CACHE_HOME")) {
      return Path(xdg_cache_home_cstr);
    } else {
      const char* home = getenv("HOME");
      return PathView(home) / ".cache";
    }
  }

  const Path& xdg_cache_home() const { return xdg_cache_home_; }

private:
  Environment(Path xdg_cache_home): xdg_cache_home_(std::move(xdg_cache_home)) {}

  Path xdg_cache_home_;
};

class Job {
public:
  Job() {}
  Job(const Job& other)=delete;
  Job(Job&& other)=default;

  Job& operator=(Job&& other)=default;

  static std::optional<Job> New(const Environment& env, PathView path) {
    std::string id = SecureRandomId();
    Path job_dir = env.xdg_cache_home() / "netpull";
    if (mkdir(job_dir.path().data(), 0755) == -1 && errno != EEXIST) {
      LogErrno("Failed to create job path");
      return {};
    }

    Path log_path = job_dir / id;
    std::fstream log(log_path.path(),
                     std::fstream::in | std::fstream::out | std::fstream::trunc);
    if (!log) {
      LogErrno("Failed to open %s", absl::FormatStreamed(log_path));
      return {};
    }

    absl::Time now = absl::Now();

    if (!(log << "# Job created at  " << now << " for :" << std::endl
              << kPathPrefix << path << std::endl)) {
      LogErrno("Failed to write to %s", absl::FormatStreamed(log_path));
      return {};
    }

    return Job(id, path, std::move(log), {});
  }

  static std::optional<Job> Load(const Environment& env, std::string id) {
    Path log_path = env.xdg_cache_home() / "netpull" / id;
    std::fstream log(log_path.path());
    if (!log) {
      LogErrno("Failed to open %s", absl::FormatStreamed(log_path));
      return {};
    }

    std::string line;
    std::string path_str;
    GuardedSet<std::string> completed_paths;

    while (std::getline(log, line)) {
      std::string_view view(line);

      if (absl::StartsWith(view, "#")) {
        continue;
      } else if (absl::StartsWith(view, kPathPrefix)) {
        view.remove_prefix(kPathPrefix.size());
        path_str = view;
      } else if (absl::StartsWith(view, kCompletedPrefix)) {
        view.remove_prefix(kCompletedPrefix.size());
        completed_paths.Insert(std::string(view));
      }
    }

    if (!log.eof()) {
      LogErrno("Failed to read %s", absl::FormatStreamed(log_path));
      return {};
    }

    log.clear();

    Path path(path_str);
    if (!path.IsResolved()) {
      LogError("%s is not a resolved path", absl::FormatStreamed(path));
      return {};
    }

    return Job(id, path, std::move(log), std::move(completed_paths));
  }

  bool IsCompleted(std::string_view path) const {
    return completed_paths.Contains(path.data());
  }

  // This function is NOT thread-safe.
  void MarkCompleted(std::string_view path) {
    completed_paths.Insert(path.data());

    if (!(log << kCompletedPrefix << path << std::endl)) {
      LogErrno("Failed to mark %s as completed", path);
    }
  }

  const std::string& id() const { return id_; }
  const Path& path() const { return path_; }

private:
  Job(std::string id, Path path, std::fstream&& log, GuardedSet<std::string> completed_paths):
    id_(std::move(id)), path_(std::move(path)), log(std::move(log)),
    completed_paths(std::move(completed_paths)) {}

  static constexpr std::string_view kPathPrefix = "Path ";
  static constexpr std::string_view kCompletedPrefix = "Completed ";

  std::string id_;
  Path path_;
  std::fstream log;
  GuardedSet<std::string> completed_paths;
};

enum class TaskPriority {
  kFileIntegrity = 0,
  kFileStream,
};

class FileIntegrityTask : public Task {
public:
  FileIntegrityTask(SubmissionKey* key, proto::PullResponse resp, absl::Mutex* conn_mutex,
                    SocketConnection* conn, Path path):
    Task(key), resp(std::move(resp)), conn_mutex(conn_mutex), conn(conn),
    path(std::move(path)) {}

  int priority() const override {
    return static_cast<int>(TaskPriority::kFileIntegrity);
  }

  void Run(WorkerPool* pool) override {
    if (!conn->alive()) {
      return;
    }

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::ifstream is(path.path());
    if (!is) {
      LogErrno("Failed to open %s", absl::FormatStreamed(path));
      return;
    }

    std::array<std::byte, Sha256Builder::kRecommendedBufferSize> buffer;
    Sha256Builder builder;

    while (is && conn->alive()) {
      is.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
      if (is.gcount()) {
        builder.Update(buffer, is.gcount());
      }
    }

    if (!conn->alive()) {
      return;
    } else if (!is.eof()) {
      LogErrno("Failed to read from %s", absl::FormatStreamed(path));
      return;
    }

    if (auto digest = builder.Finish()) {
      resp.mutable_object()->mutable_transfer()->set_sha256(*digest);

      absl::MutexLock lock(conn_mutex);
      conn->SendProtobufMessage(resp);
    }
  }

private:
  proto::PullResponse resp;
  absl::Mutex* conn_mutex;
  SocketConnection* conn;
  Path path;
};

class FileStreamTask : public Task {
public:
  FileStreamTask(SubmissionKey* key, SocketConnection conn, Path path, uint64_t bytes):
    Task(key), conn(std::move(conn)), path(path), bytes(bytes) {}

  int priority() const override {
    return static_cast<int>(TaskPriority::kFileStream);
  }

  void Run(WorkerPool* pool) override {
    ScopedFd fd;
    if (int rawfd = openat(AT_FDCWD, path.path().data(), O_RDONLY); rawfd != -1) {
      fd.reset(rawfd);
    } else {
      return;
    }

    if (posix_fadvise(*fd, 0, bytes, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED) == -1) {
      LogErrno("posix_fadvise(POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED)");
    }

    constexpr uint64_t kMaxSendfileBuffer = std::numeric_limits<ssize_t>::max();
    uint64_t bytes_remaining = bytes;

    while (bytes_remaining > 0) {
      ssize_t bytes_read = sendfile(*conn.fd(), *fd, nullptr,
                                   std::min(kMaxSendfileBuffer, bytes_remaining));
      if (bytes_read == 0) {
        LogError("sendfile of %s returned 0 while %d bytes remained",
                 absl::FormatStreamed(path), bytes);
        break;
      } else if (bytes_read == -1) {
        LogErrno("sendfile of %s", absl::FormatStreamed(path));
        return;
      }

      bytes_remaining -= bytes_read;
    }

    if (posix_fadvise(*fd, 0, bytes, POSIX_FADV_DONTNEED) == -1) {
      LogErrno("posix_fadvise(POSIX_FADV_DONTNEED)");
    }
  }

private:
  SocketConnection conn;
  Path path;
  uint64_t bytes;
};

struct ReadyFile {
  IpAddress address;
  Path path;
  uint64_t bytes;
};

void CopyTimespecToProtoTimestamp(google::protobuf::Timestamp* out, const struct timespec& ts) {
  out->set_seconds(ts.tv_sec);
  out->set_nanos(ts.tv_nsec);
}

using ReadyFilesMap = GuardedMap<std::string, ReadyFile>;

class ClientSendCrawler : public FastCrawler {
public:
  ClientSendCrawler(const Job& job, SubmissionKey* key, WorkerPool* pool,
                    ReadyFilesMap* ready_files, SocketConnection* conn):
    job(job), key(key), pool(pool), ready_files(ready_files), conn(conn){}

  int total() { return total_; }

protected:
  void HandleObject(const FsObject& object) override {
    LogVerbose("Found %s", absl::FormatStreamed(object.path()));

    PathView relative_path = PathView(object.path()).RelativeTo(PathView(object.root()));
    if (job.IsCompleted(relative_path.path())) {
      LogVerbose("Skipping %s because it's already completed",
                 absl::FormatStreamed(relative_path));
      return;
    }

    int number = ++total_;

    proto::PullResponse resp;
    proto::PullResponse::PullObject* pull = resp.mutable_object();
    pull->set_number(number);
    pull->set_path(relative_path.path().data());

    switch (object.type()) {
    case FsObject::Type::kFile:
      pull->set_type(proto::PullResponse::PullObject::kTypeFile);
      break;
    case FsObject::Type::kDirectory:
      pull->set_type(proto::PullResponse::PullObject::kTypeDirectory);
      break;
    case FsObject::Type::kSymlink:
      pull->set_type(proto::PullResponse::PullObject::kTypeSymlink);
      break;
    }

    const auto& st = object.stat();

    proto::PullResponse::PullObject::Ownership* owner = pull->mutable_owner();
    owner->set_uid(st.st_uid);
    owner->set_gid(st.st_gid);

    if (struct passwd* pwd = getpwuid(st.st_uid)) {
      owner->set_user(pwd->pw_name);
    }

    if (struct group *gr = getgrgid(st.st_gid)) {
      owner->set_group(gr->gr_name);
    }

    proto::PullResponse::PullObject::Times* times = pull->mutable_times();
    CopyTimespecToProtoTimestamp(times->mutable_access(), st.st_atim);
    CopyTimespecToProtoTimestamp(times->mutable_modify(), st.st_mtim);

    if (object.type() == FsObject::Type::kSymlink) {
      proto::PullResponse::PullObject::SymlinkData* symlink = pull->mutable_symlink();

      auto target = object.link_target();
      if (target.IsAbsolute() && target.IsChildOf(object.root())) {
        // Drop the extra slash since all these paths are absolute.
        symlink->set_target(target.RelativeTo(object.root()).path().substr(1).data());
        symlink->set_relative_to_root(true);
      } else {
        symlink->set_target(target.path().data());
      }
    } else {
      proto::PullResponse::PullObject::NonlinkData* nonlink = pull->mutable_nonlink();
      nonlink->set_perms(st.st_mode & ACCESSPERMS);
    }

    // We can't send it out kFile yet, because a Task must find its sha256 integrity first.
    if (object.type() == FsObject::Type::kFile) {
      proto::PullResponse::PullObject::FileTransferInfo* transfer = pull->mutable_transfer();

      transfer->set_bytes(st.st_size);

      auto id = SecureRandomId();
      transfer->set_id(id);
      ready_files->Put(id, {conn->peer(), object.path(), static_cast<uint64_t>(st.st_size)});

      auto task = new FileIntegrityTask(key, std::move(resp), &conn_mutex, conn, object.path());
      pool->Submit(std::unique_ptr<Task>(task));
    } else {
      absl::MutexLock lock(&conn_mutex);
      conn->SendProtobufMessage(resp);
    }
  }

private:
  const Job& job;
  SubmissionKey* key;
  WorkerPool* pool;
  ReadyFilesMap* ready_files;
  SocketConnection* conn;
  absl::Mutex conn_mutex;
  int total_ = 0;
};

// Don't pass in a ScopedFd to threads to avoid conflicts between its lifetime on the parent
// thread and on the target thread.

void ClientManagerThread(PathView root, WorkerPool* pool, ReadyFilesMap* ready_files,
                         SocketConnection conn, const Job& job) {
  SubmissionKey key;

  if (!job.path().IsAbsolute() || !job.path().IsResolved()) {
    proto::PullResponse resp;
    resp.mutable_error()->set_message("Paths must be absolute.");
    conn.SendProtobufMessage(resp);
    return;
  }

  proto::PullResponse start_resp;
  proto::PullResponse::Started* started = start_resp.mutable_started();
  started->set_job(job.id());

  if (!conn.SendProtobufMessage(start_resp)) {
    return;
  }

  ClientSendCrawler crawler(job, &key, pool, ready_files, &conn);
  crawler.Visit(root / job.path());
  key.WaitForPending();

  LogVerbose("Sending done message");

  proto::PullResponse finish_resp;
  proto::PullResponse::Finished* finished = finish_resp.mutable_finished();
  finished->set_total(crawler.total());

  conn.SendProtobufMessage(finish_resp);
}

ABSL_FLAG(bool, verbose, false, "Be verbose");
ABSL_FLAG(std::string, root, ".", "The root directory to serve");
ABSL_FLAG(int, port, IpLocation::kDefaultPort, "The default port to serve on");
ABSL_FLAG(int, workers, 4, "The default number of file streaming workers to use");
ABSL_FLAG(std::vector<IpRange>, allow_ip_ranges, {},
          "Allow IP addresses in this range, @private represents any private IP");
ABSL_FLAG(std::vector<IpRange>, deny_ip_ranges, {},
          "Deny IP addresses in this range, @private represents any private IP");

int main(int argc, char** argv) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  signal(SIGPIPE, SIG_IGN);

  std::vector<char*> c_args = absl::ParseCommandLine(argc, argv);
  if (c_args.size() != 1) {
    LogError("No arguments may be given.");
    return 1;
  }

  if (absl::GetFlag(FLAGS_verbose)) {
    EnableVerboseLogging();
  }

  auto env = Environment::Create();

  WorkerPool pool(absl::GetFlag(FLAGS_workers));
  pool.Start();

  ReadyFilesMap ready_files;

  std::string root_str = absl::GetFlag(FLAGS_root);
  auto opt_root = Path(root_str).Resolve();
  if (!opt_root) {
    return 1;
  }

  PathView root = *opt_root;

  absl::flat_hash_map<std::string, Job> jobs;

  auto allow_ip_ranges = absl::GetFlag(FLAGS_allow_ip_ranges);
  auto deny_ip_ranges = absl::GetFlag(FLAGS_deny_ip_ranges);

  if (allow_ip_ranges.empty()) {
    LogError("At least one allowed IP range must be given.");
    return 1;
  }

  IpLocation host(IpAddress(0, 0, 0, 0), absl::GetFlag(FLAGS_port));
  auto server = SocketServer::Bind(host);
  if (!server) {
    return 1;
  }

  LogInfo("Accepting connections!");

  for (;;) {
    auto conn = server->Accept();
    if (!conn) {
      break;
    }

    LogVerbose("Found connection from %s", absl::FormatStreamed(conn->peer()));

    if (IpRange::MultiContains(deny_ip_ranges, conn->peer())) {
      LogError("IP address %s was explicitly denied", absl::FormatStreamed(conn->peer()));
      continue;
    } else if (!IpRange::MultiContains(allow_ip_ranges, conn->peer())) {
      LogError("IP address %s was not allowed", absl::FormatStreamed(conn->peer()));
      continue;
    }

    proto::PullRequest req;
    if (!conn->ReadProtobufMessage(&req)) {
      return 1;
    }

    if (req.has_start() || req.has_continue_()) {
      Job job;

      if (req.has_start()) {
        Path path = req.start().path();
        LogInfo("Client %s requested to start %s", absl::FormatStreamed(conn->peer()),
                absl::FormatStreamed(path));

        if (auto opt_job = Job::New(env, path)) {
          job = std::move(*opt_job);
        } else {
          continue;
        }
      } else if (req.has_continue_()) {
        std::string job_id = req.continue_().job();
        if (auto opt_job = Job::Load(env, job_id)) {
          job = std::move(*opt_job);
        } else {
          continue;
        }
      } else {
        assert(false);
      }

      std::string id = job.id();
      jobs.emplace(id, std::move(job));
      std::thread(ClientManagerThread, root, &pool, &ready_files, std::move(*conn),
                  std::ref(jobs[id])).detach();
    } else if (req.has_file()) {
      std::string id = req.file().id();
      LogVerbose("Client %s requested file %s", absl::FormatStreamed(conn->peer()), id);

      auto opt_ready = ready_files.GetAndPopIf(id, [&](const ReadyFile& ready) -> bool {
        return conn->peer() == ready.address;
      });

      if (opt_ready) {
        auto task = new FileStreamTask(nullptr, std::move(*conn), std::move(opt_ready->path),
                                       opt_ready->bytes);
        pool.Submit(std::unique_ptr<Task>(task));
      }
    } else if (req.has_success()) {
      auto it = jobs.find(req.success().job());
      if (it == jobs.end()) {
        LogError("Missing job ID: %s", req.success().job());
        continue;
      }

      auto& job = it->second;
      job.MarkCompleted(req.success().path());
    }
  }

  return 0;
}
