/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "netpull/console.h"

#include "socket.h"

namespace netpull {

static std::optional<ScopedFd> CreateEmptyIpv4Socket(int priority) {
  if (int rawfd = socket(AF_INET, SOCK_STREAM, 0); rawfd != -1) {
    ScopedFd fd(rawfd);

    if (priority != SocketConnection::kNoPriority) {
      if (setsockopt(*fd, SOL_SOCKET, SO_PRIORITY, reinterpret_cast<void*>(&priority),
                     sizeof(priority)) == -1) {
        LogWarning("Failed to setsockopt(SO_PRIORITY, %d)", priority);
      }
    }

    return fd;
  } else {
    LogErrno("Failed to create socket");
    return {};
  }
}

std::optional<SocketServer> SocketServer::Bind(IpLocation location, int priority) {
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  if (!location.ToUnixSockaddr(&sockaddr)) {
    return {};
  }

  if (auto opt_socket = CreateEmptyIpv4Socket(priority)) {
    ScopedFd fd = std::move(*opt_socket);

    int reuseaddr = 1;
    if (setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void*>(&reuseaddr),
                   sizeof(reuseaddr)) == -1) {
      LogWarning("Failed to setsockopt(SO_REUSEADDR)");
    }

    if (bind(*fd, reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr)) == -1) {
      LogErrno("Failed to bind to %s", absl::FormatStreamed(location));
      return {};
    }

    if (listen(*fd, SOMAXCONN) == -1) {
      LogErrno("Failed to listen on %s", absl::FormatStreamed(location));
      return {};
    }

    return SocketServer(std::move(location), std::move(fd));
  } else {
    return {};
  }
}

std::optional<SocketConnection> SocketServer::Accept() {
  struct sockaddr_in sockaddr;
  socklen_t sockaddr_len = sizeof(sockaddr);

  if (int rawfd = accept(*fd_, reinterpret_cast<struct sockaddr*>(&sockaddr), &sockaddr_len);
      rawfd != -1) {
    ScopedFd fd(rawfd);

    if (auto opt_addr = IpAddress::FromUnixSockaddr(&sockaddr)) {
      return SocketConnection(std::move(*opt_addr), std::move(fd));
    }
  } else {
    LogErrno("Failed to accept connection on %s", absl::FormatStreamed(bound_));
  }

  return {};
}

std::optional<SocketConnection> SocketConnection::Connect(IpLocation location, int priority) {
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  if (!location.ToUnixSockaddr(&sockaddr)) {
    return {};
  }

  if (auto opt_socket = CreateEmptyIpv4Socket(priority)) {
    ScopedFd fd = std::move(*opt_socket);

    if (connect(*fd, reinterpret_cast<struct sockaddr*>(&sockaddr), sizeof(sockaddr)) == -1) {
      LogErrno("Failed to connect to %s", absl::FormatStreamed(location));
    } else {
      return SocketConnection(location.address(), std::move(fd));
    }
  }

  return {};
}

std::optional<size_t> SocketConnection::ReadBytes(std::byte* bytes, size_t size) {
  for (;;) {
    ssize_t bytes_read = recv(*fd_, reinterpret_cast<void*>(bytes), size, 0);
    if (bytes_read >= 0) {
      return static_cast<size_t>(bytes_read);
    } else if (bytes_read == -1 && errno != EINTR) {
      LogErrno("Failed to read from connection with %s", absl::FormatStreamed(peer_));
      if (errno == EPIPE) {
        alive_ = false;
      }
      return {};
    }
  }
}

bool SocketConnection::ReadAllBytes(std::byte* bytes, size_t size) {
  for (size_t total_read = 0; total_read < size; ) {
    if (auto opt_bytes_read = ReadBytes(bytes + total_read, size - total_read)) {
      if (*opt_bytes_read == 0) {
        LogError("Expected to read %d bytes but only found %d", size, total_read);
        return false;
      }
      total_read += *opt_bytes_read;
    } else {
      return false;
    }
  }

  return true;
}

std::optional<size_t> SocketConnection::SendBytes(const std::byte* bytes, size_t size) {
  for (;;) {
    ssize_t bytes_written = send(*fd_, reinterpret_cast<const void*>(bytes), size, 0);
    if (bytes_written >= 0) {
      return static_cast<size_t>(bytes_written);
    } else if (bytes_written == -1 && errno != EINTR) {
      LogErrno("Failed to write to connection with %s", absl::FormatStreamed(peer_));
      if (errno == EPIPE) {
        alive_ = false;
      }
      return {};
    }
  }
}

bool SocketConnection::SendAllBytes(const std::byte* bytes, size_t size) {
  for (size_t total_written = 0; total_written < size; ) {
    if (auto opt_bytes_written = SendBytes(bytes + total_written, size - total_written)) {
      total_written += *opt_bytes_written;
    } else {
      return false;
    }
  }

  return true;
}

bool SocketConnection::ReadProtobufMessage(google::protobuf::Message* message) {
  uint32_t size;

  if (!ReadAllBytes(reinterpret_cast<std::byte*>(&size), sizeof(size))) {
    return false;
  }

  std::string buffer(htonl(size), '\0');
  if (!ReadAllBytes(reinterpret_cast<std::byte*>(buffer.data()), buffer.size())) {
    return false;
  }

  return message->ParseFromString(buffer);
}

bool SocketConnection::SendProtobufMessage(const google::protobuf::Message& message) {
  uint32_t size = ntohl(message.ByteSizeLong());

  if (!SendAllBytes(reinterpret_cast<std::byte*>(&size), sizeof(size))) {
    return false;
  }

  errno = 0;
  if (message.SerializeToFileDescriptor(*fd_)) {
    return true;
  } else if (errno) {
    LogErrno("Failed to send protobuf message to %s", absl::FormatStreamed(peer_));
  } else {
    LogError("Unknown error occurred sending protobuf message to %s",
             absl::FormatStreamed(peer_));
  }

  return false;
}

}  // namespace netpull
