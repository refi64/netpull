/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>
#include <string_view>

#include "netpull/client/utf8.h"

namespace netpull {

// A ProgressBuilder is responsible for building a progress line. Given an action symbol (which
// should be a string containing a single one-column-wide UTF32 charater, such as a download
// symbol) and item string that is being processed, it will build a progress bar on each call
// to BuildLine.
class ProgressBuilder {
public:
  ProgressBuilder(std::string_view action, std::string_view item);

  // Given the current progress (a value from [0:1]), return a progress line containing both
  // the item and a progress bar filled to the given percentage.
  std::string BuildLine(double progress) const;

private:
  std::string_view action;
  std::string_view item;
  const Utf8WidthInformation item_utf8;
};

}  // namespace netpull
