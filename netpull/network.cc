/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

#include "netpull/console.h"

#include "netpull/network.h"

namespace netpull {

std::optional<IpAddress> IpAddress::FromUnixSockaddr(struct sockaddr_in* sockaddr) {
  std::array<char, INET_ADDRSTRLEN> buffer;
  if (inet_ntop(AF_INET, reinterpret_cast<void*>(&sockaddr->sin_addr), buffer.data(),
                buffer.size()) == nullptr) {
      LogErrno("inet_ntop");
      return {};
  }

  return IpAddress::Parse(buffer.data());
}

std::optional<IpAddress> IpAddress::Parse(std::string_view input) {
  std::vector<std::string> str_parts = absl::StrSplit(input, '.');
  if (str_parts.size() != 4) {
    return {};
  }

  std::vector<int> parts;
  for (const auto& str_part : str_parts) {
    int part;
    try {
      part = std::stoi(str_part);
    } catch (std::exception&) {
      return {};
    }

    if (part < 0 || part > 255) {
      return {};
    }

    parts.push_back(part);
  }

  return IpAddress{{parts[0], parts[1], parts[2], parts[3]}};
}

bool IpAddress::ToUnixSockaddr(struct sockaddr_in* sockaddr) {
  std::string str = absl::StrJoin(parts(), ".");
  if (inet_pton(AF_INET, str.c_str(), reinterpret_cast<void*>(&sockaddr->sin_addr)) <= 0) {
    LogErrno("inet_pton of %s", str);
    return false;
  }

  return true;
}

bool IpAddress::GenericLessThan(const IpAddress& other, bool or_equal_to) const {
  for (int i = 0; i < kNumParts; i++) {
    if (parts_[i] < other.parts_[i]) {
      break;
    } else if (parts_[i] > other.parts_[i] ||
    // If we haven't left yet but we're on the last part, and it's equal, then
    // the entire thing is equal.
    (i == kNumParts - 1 && !or_equal_to && parts_[i] == other.parts_[i])) {
      return false;
    }
  }

  return true;
}

std::optional<IpRange> IpRange::Parse(std::string_view input) {
  std::vector<std::string> str_parts = absl::StrSplit(input, '-');

  if (str_parts.size() != 1 && str_parts.size() != 2) {
    return {};
  }

  if (auto opt_addr_start = IpAddress::Parse(str_parts[0])) {
    if (str_parts.size() == 2) {
      if (auto opt_addr_end = IpAddress::Parse(str_parts[1])) {
        return IpRange{*opt_addr_start, *opt_addr_end};
      } else {
        return {};
      }
    } else {
      return IpRange{*opt_addr_start};
    }
  } else {
    return {};
  }
}

std::optional<std::vector<IpRange>> IpRange::ParseMulti(std::string_view input) {
  if (input == "@private") {
    return kPrivateIpRanges;
  } else if (input.empty()) {
    return {{}};
  }

  std::vector<std::string> str_parts = absl::StrSplit(input, ',');
  std::vector<IpRange> result;
  for (const auto& part : str_parts) {
    if (auto range_opt = IpRange::Parse(part)) {
      result.push_back(*range_opt);
    } else {
      return {};
    }
  }

  return result;
}

const std::vector<IpRange> IpRange::kPrivateIpRanges{
  IpRange{{10, 0, 0, 0}, {{10, 255, 255, 255}}},
  IpRange{{172, 16, 0, 0}, {{172, 31, 255, 255}}},
  IpRange{{192, 168, 0, 0}, {{192, 168, 255, 255}}},
  IpRange{{127, 0, 0, 0}, {{127, 255, 255, 255}}},
};

std::optional<IpLocation> IpLocation::Parse(std::string_view input) {
  std::vector<std::string> str_parts = absl::StrSplit(input, ':');

  if (str_parts.size() != 1 && str_parts.size() != 2) {
    return {};
  }

  if (auto opt_addr = IpAddress::Parse(str_parts[0])) {
    int port = IpLocation::kDefaultPort;

    if (str_parts.size() == 2) {
      try {
        port = std::stoi(str_parts[1]);
      } catch (std::exception&) {
        return {};
      }
    }

    return IpLocation{*opt_addr, port};
  } else {
    return {};
  }
}

bool IpLocation::ToUnixSockaddr(struct sockaddr_in* sockaddr) {
  if (!address_.ToUnixSockaddr(sockaddr)) {
    return false;
  }

  sockaddr->sin_port = htons(port_);
  return true;
}

std::ostream& operator<<(std::ostream& os, const IpAddress& addr) {
  auto parts = addr.parts();
  os << absl::StrJoin(parts.begin(), parts.end(), ".");
  return os;
}

std::ostream& operator<<(std::ostream& os, const IpRange& range) {
  os << range.start();
  if (auto opt_end = range.end()) {
    os << '-' << *opt_end;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const IpLocation& location) {
  os << location.address() << ':' << location.port();
  return os;
}

bool AbslParseFlag(std::string_view text, IpLocation* result, std::string* error) {
  if (auto opt_location = IpLocation::Parse(text)) {
    *result = *opt_location;
    return true;
  } else {
    *error = "Invalid IP location";
    return false;
  }
}

std::string AbslUnparseFlag(const IpLocation& location) {
  return absl::StrFormat("%s:%d", absl::FormatStreamed(location.address()), location.port());
}

bool AbslParseFlag(std::string_view text, std::vector<IpRange>* result, std::string* error) {
  if (auto opt_ranges = IpRange::ParseMulti(text)) {
    *result = *opt_ranges;
    return true;
  } else {
    *error = "Invalid IP ranges";
    return false;
  }
}

std::string AbslUnparseFlag(const std::vector<IpRange>& ranges) {
  return absl::StrJoin(ranges, ", ", absl::StreamFormatter());
}

std::optional<ScopedFd> CreateEmptyIpv4Socket() {
  if (int rawfd = socket(AF_INET, SOCK_STREAM, 0); rawfd != -1) {
    return ScopedFd(rawfd);
  } else {
    LogErrno("Failed to create socket");
    return {};
  }
}

std::optional<SocketServer> SocketServer::Bind(IpLocation location) {
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  if (!location.ToUnixSockaddr(&sockaddr)) {
    return {};
  }

  if (auto opt_socket = CreateEmptyIpv4Socket()) {
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
  socklen_t sockaddr_len;

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

std::optional<SocketConnection> SocketConnection::Connect(IpLocation location) {
  struct sockaddr_in sockaddr;
  sockaddr.sin_family = AF_INET;
  if (!location.ToUnixSockaddr(&sockaddr)) {
    return {};
  }

  if (auto opt_socket = CreateEmptyIpv4Socket()) {
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
