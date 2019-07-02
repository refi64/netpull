/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"

#include "path.h"

namespace netpull {

std::string path_internal::Join(std::string_view self, std::string_view other) {
  bool we_have_slash = absl::EndsWith(self, "/");
  bool other_has_slash = absl::StartsWith(other, "/");

  std::string p(self);

  if (we_have_slash && other_has_slash) {
    other.remove_prefix(1);
  } else if (!we_have_slash && !other_has_slash) {
    p.push_back('/');
  }

  p += other;
  return p;
}

bool path_internal::IsAbsolute(std::string_view self) {
  return absl::StartsWith(self, "/");
}

bool path_internal::IsResolved(std::string_view self) {
  if (IsAbsolute(self)) {
    if (self == "/") {
      return true;
    }

    self.remove_prefix(1);
  }

  std::vector<std::string_view> parts = absl::StrSplit(self, '/');
  for (auto part : parts) {
    if (part.empty() || part == "." || part == "..") {
      return false;
    }
  }

  return true;
}

bool path_internal::IsChildOf(std::string_view self, std::string_view other) {
  assert(IsAbsolute(self));
  assert(IsResolved(self));
  assert(IsAbsolute(other));
  assert(IsResolved(other));
  return absl::StartsWith(self, other);
}

std::ostream& operator<<(std::ostream& os, PathView path) {
  return os << path.path();
}

std::ostream& operator<<(std::ostream& os, Path path) {
  return os << path.path();
}

}  // namespace netpull
