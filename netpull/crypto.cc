/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <array>
#include <fstream>

#include "absl/strings/str_cat.h"
#include "openssl/rand.h"

#include "netpull/console.h"

#include "netpull/crypto.h"

namespace netpull {

template <size_t N>
static std::string Hexlify(const std::array<std::byte, N>& bytes) {
  std::string result;
  result.reserve(bytes.size() * 2);

  for (auto byte : bytes) {
    result += absl::StrCat(absl::Hex(byte, absl::kZeroPad2));
  }

  return result;
}

std::string SecureRandomId() {
  constexpr int kLength = 8;
  std::array<std::byte, kLength> bytes;
  RAND_bytes(reinterpret_cast<std::uint8_t*>(bytes.data()), kLength);
  return Hexlify(bytes);
}

Sha256Builder::Sha256Builder() {
 SHA256_Init(&ctx);
}

void Sha256Builder::Update(const void* bytes, size_t size) {
  SHA256_Update(&ctx, bytes, size);
}

std::optional<std::string> Sha256Builder::Finish() {
  std::array<std::byte, SHA256_DIGEST_LENGTH> digest;
  if (!SHA256_Final(reinterpret_cast<uint8_t*>(digest.data()), &ctx)) {
    LogError("sha256 digest generation failed");
    return {};
  }

  return Hexlify(digest);
}

}  // namespace netpull
