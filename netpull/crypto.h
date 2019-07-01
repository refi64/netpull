/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "openssl/sha.h"

namespace netpull {

std::string SecureRandomId();

class Sha256Builder {
public:
  static constexpr int kRecommendedBufferSize = 64 * 1024;

  Sha256Builder();

  void Update(const void* bytes, size_t size);
  template <size_t N>
  void Update(const std::array<std::byte, N>& bytes, size_t size = 0) {
    Update(bytes.data(), size == 0 ? bytes.size() : size);
  }

  std::optional<std::string> Finish();

private:
  SHA256_CTX ctx;
};

}
