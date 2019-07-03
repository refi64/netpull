/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "openssl/sha.h"

namespace netpull {

// Generate a secure random ID.
std::string SecureRandomId();

// Incrementally builds an SHA256 digest.
class Sha256Builder {
public:
  // The recommended amount of data to pass to Sha256Builder.Update.
  static constexpr int kRecommendedBufferSize = 64 * 1024;

  Sha256Builder();

  // Process the given bytes to add to the sha256 hash.
  void Update(const void* bytes, size_t size);
  template <size_t N>
  void Update(const std::array<std::byte, N>& bytes, size_t size = 0) {
    Update(bytes.data(), size == 0 ? bytes.size() : size);
  }

  // Return an sha256 digest of the provided bytes, or an empty optional if failure.
  std::optional<std::string> Finish();

private:
  SHA256_CTX ctx;
};

}
