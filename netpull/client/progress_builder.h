/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>
#include <string_view>

#include "netpull/client/utf8.h"

namespace netpull {

class ProgressBuilder {
public:
  ProgressBuilder(std::string_view action, std::string_view item);

  std::string BuildLine(double progress) const;

private:
  std::string_view action;
  std::string_view item;
  const Utf8WidthInformation item_utf8;
};

}  // namespace netpull
