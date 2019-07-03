/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <string_view>
#include <vector>

#include "absl/types/span.h"

namespace netpull {

// Utf8WidthInformation contains Unicode character information for a UTF-8 string. For each
// Unicode character in the given string, it will calculate information about it stored in
// CharacterInfo.
class Utf8WidthInformation {
public:
  struct CharacterInfo {
    // Character's index in the UTF-8 byte string.
    size_t index;
    // Character's terminal width (amount of columns it would take to print).
    int width;
  };

  // Returns the collected character information.
  absl::Span<const CharacterInfo> characters() const { return characters_; }
  // Returns the total terminal width of the string.
  size_t total_width() const { return total_width_; }

  // Calculates the Utf8WidthInformation for the given string_view.
  static Utf8WidthInformation ForString(std::string_view str);

private:
  Utf8WidthInformation() {}

  std::vector<CharacterInfo> characters_;
  size_t total_width_ = 0;
};

}  // namespace netpull
