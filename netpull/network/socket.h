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

class SocketConnection {
public:
  static std::optional<SocketConnection> Connect(IpLocation location);

  std::optional<size_t> ReadBytes(std::byte* bytes, size_t size);
  template <int N>
  std::optional<size_t> ReadBytes(std::array<std::byte, N>* bytes) {
    return ReadBytes(bytes->data(), N);
  }
  bool ReadAllBytes(std::byte* bytes, size_t size);
  template <int N>
  bool ReadAllBytes(std::array<std::byte, N>* bytes) {
    return ReadAllBytes(bytes->data(), N);
  }

  std::optional<size_t> SendBytes(const std::byte* bytes, size_t size);
  template <int N>
  std::optional<size_t> SendBytes(const std::array<std::byte, N>& bytes) {
    return SendBytes(bytes.data(), N);
  }
  bool SendAllBytes(const std::byte* bytes, size_t size);
  template <int N>
  bool SendAllBytes(const std::array<std::byte, N>& bytes) {
    return SendAllBytes(bytes.data(), N);
  }

  bool ReadProtobufMessage(google::protobuf::Message* message);
  bool SendProtobufMessage(const google::protobuf::Message& message);

  const IpAddress& peer() const { return peer_; }
  bool alive() { return alive_; }
  const ScopedFd& fd() const { return fd_; }

private:
  SocketConnection(IpAddress peer, ScopedFd fd): peer_(std::move(peer)), fd_(std::move(fd)) {}

  IpAddress peer_;
  bool alive_ = true;
  ScopedFd fd_;

  friend class SocketServer;
};

class SocketServer {
public:
  SocketServer(const SocketServer& other)=delete;
  SocketServer(SocketServer&& other)=default;

  static std::optional<SocketServer> Bind(IpLocation location);
  std::optional<SocketConnection> Accept();

  const IpLocation& bound() const { return bound_; }
  const ScopedFd& fd() const { return fd_; }

private:
  SocketServer(IpLocation bound, ScopedFd fd): bound_(std::move(bound)), fd_(std::move(fd)) {}

  IpLocation bound_;
  ScopedFd fd_;
};

}  // namespace netpull
