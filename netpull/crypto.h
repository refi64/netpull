/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace netpull {

std::string RandomId();
std::optional<std::string> Sha256ForPath(std::string_view path);

}
