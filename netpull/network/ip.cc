/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

#include "netpull/console.h"

#include "ip.h"

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

}  // namespace netpull
