/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <string_view>
#include <vector>

namespace netpull {

class Utf8WidthInformation {
public:
  struct CharacterInfo {
    size_t index;
    int width;
  };

  const std::vector<CharacterInfo>& characters() const { return characters_; }
  size_t total_width() const { return total_width_; }

  static Utf8WidthInformation ForString(std::string_view str);

private:
  Utf8WidthInformation() {}

  std::vector<CharacterInfo> characters_;
  size_t total_width_ = 0;
};

}  // namespace netpull
