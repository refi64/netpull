/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include "google/protobuf/message.h"

#include "netpull/network/ip.h"
#include "netpull/scoped_resource.h"

namespace netpull {

// A connection to a peer, whether it be a connection to a client from the server, or a
// connection to a server from a client.
class SocketConnection {
public:
  // Represents that the connection has no defined priority.
  static constexpr int kNoPriority = -1;

  // Connect to the given IpLocation, returning an empty optional if the connection fails.
  static std::optional<SocketConnection> Connect(IpLocation location,
                                                 int priority = kNoPriority);

  // Read size bytes (or less) from the connection, returning the number of bytes successfully
  // read, or an empty optional on failure.
  std::optional<size_t> ReadBytes(std::byte* bytes, size_t size);
  template <int N>
  std::optional<size_t> ReadBytes(std::array<std::byte, N>* bytes) {
    return ReadBytes(bytes->data(), N);
  }

  // Read exactly size bytes (no less) from the connection, returning false if the full value
  // could not be received.
  bool ReadAllBytes(std::byte* bytes, size_t size);
  template <int N>
  bool ReadAllBytes(std::array<std::byte, N>* bytes) {
    return ReadAllBytes(bytes->data(), N);
  }

  // Send size bytes (or less) to the connection's peer, returning the number of bytes
  // successfully sent, or an empty optional on failure.
  std::optional<size_t> SendBytes(const std::byte* bytes, size_t size);
  template <int N>
  std::optional<size_t> SendBytes(const std::array<std::byte, N>& bytes) {
    return SendBytes(bytes.data(), N);
  }

  // Send exactly size bytes (no less) to the peer, returning false if the full value could not
  // be sent.
  bool SendAllBytes(const std::byte* bytes, size_t size);
  template <int N>
  bool SendAllBytes(const std::array<std::byte, N>& bytes) {
    return SendAllBytes(bytes.data(), N);
  }

  // Read a protobuf message size and full message from the connection, returning false on
  // failure.
  bool ReadProtobufMessage(google::protobuf::Message* message);
  // Send a protobuf message size and full message to the connection, returning false on
  // failure.
  bool SendProtobufMessage(const google::protobuf::Message& message);

  // Get the IpAddress of the connection's peer.
  const IpAddress& peer() const { return peer_; }
  // Is the connection alive? (false if any previous operations resulted in a SIGPIPE.)
  bool alive() { return alive_; }
  // Get the fd associated with the connection.
  const ScopedFd& fd() const { return fd_; }

private:
  SocketConnection(IpAddress peer, ScopedFd fd): peer_(std::move(peer)), fd_(std::move(fd)) {}

  IpAddress peer_;
  bool alive_ = true;
  ScopedFd fd_;

  friend class SocketServer;
};

// A server listening on some network location, which can accept connections from a client.
class SocketServer {
public:
  SocketServer(const SocketServer& other)=delete;
  SocketServer(SocketServer&& other)=default;

  // Bind to an IpLocation to listen for connections, returning an empty optional on failure.
  static std::optional<SocketServer> Bind(IpLocation location,
                                          int priority = SocketConnection::kNoPriority);
  // Accept a connection from a client, returning an empty optional on failure.
  std::optional<SocketConnection> Accept();

  // Get the location the server is bound to.
  const IpLocation& bound() const { return bound_; }
  // Get the server's fd.
  const ScopedFd& fd() const { return fd_; }

private:
  SocketServer(IpLocation bound, ScopedFd fd): bound_(std::move(bound)), fd_(std::move(fd)) {}

  IpLocation bound_;
  ScopedFd fd_;
};

}  // namespace netpull
