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

// An IpAddress encapsulates an IPv4 address.
class IpAddress {
public:
  static const int kNumParts = 4;

  IpAddress(std::array<int, 4> parts): parts_(parts) {}
  IpAddress(int a, int b, int c, int d): parts_{a, b, c, d} {}

  // Create an IpAddress from a sockaddr_in structure, returning an empty optional if it fails.
  static std::optional<IpAddress> FromUnixSockaddr(struct sockaddr_in* sockaddr);
  // Parse an IpAddress from an input string, returning an empty optional if it fails.
  static std::optional<IpAddress> Parse(std::string_view input);

  // Returns the 4 different parts of the IP address.
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

  // Converts the given IP address to a sockaddr_in structure, returning false if the
  // conversion fails.
  bool ToUnixSockaddr(struct sockaddr_in* sockaddr);

private:
  bool GenericLessThan(const IpAddress& other, bool or_equal_to) const;

  std::array<int, kNumParts> parts_;
};

// An IpRange contains either one or a range of IPv4 address.
class IpRange {
public:
  IpRange(IpAddress start, std::optional<IpAddress> end = {}): start_(start), end_(end) {}

  // Parse an IpRange, returning an empty optional if the parsing fails.
  // The syntax is XXX.XXX.XXX.XXX-YYY.YYY.YYY.YYY, or just one IP if the end is unspecified.
  static std::optional<IpRange> Parse(std::string_view input);
  // Parse a sequence of IpRanges, separated by commas. Returns an empty optional if the
  // parsing fails.
  static std::optional<std::vector<IpRange>> ParseMulti(std::string_view input);

  // The start of the IpRange.
  const IpAddress& start() const {
    return start_;
  }

  // The end of the IpRange, if present.
  const std::optional<IpAddress>& end() const {
    return end_;
  }

  // Does this IpRange contain the given IpAddress?
  bool Contains(const IpAddress& ip) const {
    if (end_) {
      return start_ <= ip && ip <= *end_;
    } else {
      return start_ == ip;
    }
  }

  // Does the sequence of IpRanges contain the given IpAddress?
  static bool MultiContains(const std::vector<IpRange>& ranges, const IpAddress& ip) {
    for (const auto& range : ranges) {
      if (range.Contains(ip)) {
        return true;
      }
    }

    return false;
  }

  // A const vector of IpRanges that contains all IPs on the private network.
  static const std::vector<IpRange> kPrivateIpRanges;

private:
  IpAddress start_;
  std::optional<IpAddress> end_;
};

// An IpLocation represents an IpAddress paired with a port, representing a network location
// that can be connected to.
class IpLocation {
public:
  // The default netpull port.
  static constexpr int kDefaultPort = 7420;

  IpLocation(IpAddress address, int port = kDefaultPort): address_(address), port_(port) {}

  // Parse an IpLocation from an IP address and optional port, separated by a :. If no port is
  // given, kDefaultPort is used. If a parse error occurs, an empty optional is returned.
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

  // Converts the IpLocation to a sockaddr_in structure, returning false if the conversion
  // fails.
  bool ToUnixSockaddr(struct sockaddr_in* sockaddr);

private:
  IpAddress address_;
  int port_;
};

// Printing and flag parsing helpers.

std::ostream& operator<<(std::ostream& os, const IpAddress& addr);
std::ostream& operator<<(std::ostream& os, const IpRange& range);
std::ostream& operator<<(std::ostream& os, const IpLocation& location);

bool AbslParseFlag(std::string_view text, IpLocation* location, std::string* error);
std::string AbslUnparseFlag(const IpLocation& location);
bool AbslParseFlag(std::string_view text, std::vector<IpRange>* result, std::string* error);
std::string AbslUnparseFlag(const std::vector<IpRange>& ranges);

}  // namespace netpull
