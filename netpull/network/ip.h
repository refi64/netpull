/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <arpa/inet.h>

#include <array>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace netpull {

class IpAddress {
public:
  static const int kNumParts = 4;

  IpAddress(std::array<int, 4> parts): parts_(parts) {}
  IpAddress(int a, int b, int c, int d): parts_{a, b, c, d} {}

  static std::optional<IpAddress> FromUnixSockaddr(struct sockaddr_in* sockaddr);
  static std::optional<IpAddress> Parse(std::string_view input);

  const std::array<int, 4>& parts() const {
    return parts_;
  }

  bool operator==(const IpAddress& other) const {
    return parts() == other.parts();
  }

  bool operator!=(const IpAddress& other) const {
    return !(*this == other);
  }

  bool operator<(const IpAddress& other) const {
    return GenericLessThan(other, false);
  }

  bool operator<=(const IpAddress& other) const {
    return GenericLessThan(other, true);
  }

  bool operator>(const IpAddress& other) const {
    return !(*this <= other);
  }

  bool operator>=(const IpAddress& other) const {
    return !(*this < other);
  }

  bool ToUnixSockaddr(struct sockaddr_in* sockaddr);

private:
  bool GenericLessThan(const IpAddress& other, bool or_equal_to) const;

  std::array<int, kNumParts> parts_;
};

class IpRange {
public:
  IpRange(IpAddress start, std::optional<IpAddress> end = {}): start_(start), end_(end) {}

  static std::optional<IpRange> Parse(std::string_view input);
  static std::optional<std::vector<IpRange>> ParseMulti(std::string_view input);

  const IpAddress& start() const {
    return start_;
  }

  const std::optional<IpAddress>& end() const {
    return end_;
  }

  bool Contains(const IpAddress& ip) const {
    if (end_) {
      return start_ <= ip && ip <= *end_;
    } else {
      return start_ == ip;
    }
  }

  static bool MultiContains(const std::vector<IpRange>& ranges, const IpAddress& ip) {
    for (const auto& range : ranges) {
      if (range.Contains(ip)) {
        return true;
      }
    }

    return false;
  }

  static const std::vector<IpRange> kPrivateIpRanges;

private:
  IpAddress start_;
  std::optional<IpAddress> end_;
};

class IpLocation {
public:
  static constexpr int kDefaultPort = 7420;

  IpLocation(IpAddress address, int port = kDefaultPort): address_(address), port_(port) {}

  static std::optional<IpLocation> Parse(std::string_view input);

  const IpAddress& address() const {
    return address_;
  }

  int port() const {
    return port_;
  }

  bool operator==(const IpLocation& other) {
    return address_ == other.address_ && port_ == other.port_;
  }

  bool operator!=(const IpLocation& other) {
    return !(*this == other);
  }

  bool ToUnixSockaddr(struct sockaddr_in* sockaddr);

private:
  IpAddress address_;
  int port_;
};

std::ostream& operator<<(std::ostream& os, const IpAddress& addr);
std::ostream& operator<<(std::ostream& os, const IpRange& range);
std::ostream& operator<<(std::ostream& os, const IpLocation& location);

bool AbslParseFlag(std::string_view text, IpLocation* location, std::string* error);
std::string AbslUnparseFlag(const IpLocation& location);
bool AbslParseFlag(std::string_view text, std::vector<IpRange>* result, std::string* error);
std::string AbslUnparseFlag(const std::vector<IpRange>& ranges);

}  // namespace netpull
