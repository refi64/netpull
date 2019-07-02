/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// XXX: Avoid weird issues with differing exception specifications.
#include <wchar.h>

#include "wcwidth.h"

#include "netpull/console.h"

#include "netpull/client/utf8.h"

namespace netpull {

Utf8WidthInformation Utf8WidthInformation::ForString(std::string_view str) {
  static_assert(sizeof(wchar_t) == 4);

  Utf8WidthInformation info;

  // We're going to do bit manipulations, so convert it to unsigned.
  std::basic_string_view<unsigned char> ustr(reinterpret_cast<const unsigned char*>(str.data()),
                                             str.size());

  constexpr int
    kOneByteMask = 0x80,
    kOneByteValue = 0,
    kTwoByteMask = 0xE0,
    kTwoByteValue = 0xC0,
    kThreeByteMask = 0xF0,
    kThreeByteValue = kTwoByteMask,
    kFourByteMask = 0xF8,
    kFourByteValue = kThreeByteMask,
    kSequenceMask = 0x3F;

  wchar_t current_char = 0;
  for (auto it = ustr.begin(); it != ustr.end(); ) {
    unsigned char c = *it;
    size_t pos = it - ustr.begin();

    if ((c & kOneByteMask) == kOneByteValue) {
      current_char = *it++;
    } else if ((c & kTwoByteMask) == kTwoByteValue) {
      if (ustr.end() - it < 2) {
        continue;
      }

      current_char = (*it++ & ~kTwoByteMask) << 6;
      current_char |= *it++ & kSequenceMask;
    } else if ((c & kThreeByteMask) == kThreeByteValue) {
      if (ustr.end() - it < 3) {
        continue;
      }

      current_char = (*it++ & ~kThreeByteMask) << 12;
      current_char |= (*it++ & kSequenceMask) << 6;
      current_char |= *it++ & kSequenceMask;
    } else if ((c & kFourByteMask) == kFourByteValue) {
      if (ustr.end() - it < 4) {
        continue;
      }

      current_char = (*it++ & ~kFourByteMask) << 18;
      current_char |= (*it++ & kSequenceMask) << 12;
      current_char |= (*it++ & kSequenceMask) << 6;
      current_char |= *it++ & kSequenceMask;
    }

    int width = wcwidth(current_char);
    if (width == -1) {
      LogWarning("wcwidth of %s (%d) returned -1 [%d:%d)",
                 std::string_view(str.data() + pos, it - ustr.begin() - pos), current_char,
                 pos, it - ustr.begin());
      width = 0;
    }

    info.characters_.push_back({pos, width});
    info.total_width_ += width;
  }

  return info;
}

} // namespace netpull
