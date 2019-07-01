/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <array>
#include <fstream>

#include "absl/strings/str_cat.h"
#include "openssl/rand.h"
#include "openssl/sha.h"

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

std::string RandomId() {
  constexpr int kLength = 8;
  std::array<std::byte, kLength> bytes;
  RAND_bytes(reinterpret_cast<std::uint8_t*>(bytes.data()), kLength);
  return Hexlify(bytes);
}

std::optional<std::string> Sha256ForPath(std::string_view path) {
  SHA256_CTX ctx;
  SHA256_Init(&ctx);

  std::ifstream is(path.data());
  if (!is) {
    LogErrno("Failed to open %s", path);
    return {};
  }

  constexpr int kBufferSize = 64 * 1024;
  std::array<char, kBufferSize> buffer;

  while (is) {
    is.read(buffer.data(), buffer.size());
    if (is.gcount()) {
      SHA256_Update(&ctx, reinterpret_cast<const void*>(buffer.data()), is.gcount());
    }
  }

  if (!is.eof()) {
    LogErrno("Failed to read from %s", path);
    return {};
  }

  std::array<std::byte, SHA256_DIGEST_LENGTH> digest;
  if (!SHA256_Final(reinterpret_cast<uint8_t*>(digest.data()), &ctx)) {
    LogError("sha256 digest generation for %s failed", path);
    return {};
  }

  return Hexlify(digest);
}

}
