/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "netpull/console.h"

#include "netpull/client/progress_builder.h"

namespace netpull {

ProgressBuilder::ProgressBuilder(std::string_view action, std::string_view item):
    action(action), item(item), item_utf8(Utf8WidthInformation::ForString(item)) {
  assert(Utf8WidthInformation::ForString(action).total_width() == 1);
}

std::string ProgressBuilder::BuildLine(double progress) const {
  struct winsize ws;
  int columns = 75;
  if (ioctl(1, TIOCGWINSZ, &ws) == -1) {
    LogErrno("ioctl(1, TIOCGWINSZ)");
  } else {
    columns = ws.ws_col;
  }

  // Leave an extra space.
  columns--;

  // (iostreams aren't the fastest here and not too useful, might as well build it ourselves.)
  // XXX: trying to get a decent-length buffer size, assume any char may be full-width UTF-8.
  std::string result(std::max(columns * 4, 15), ' ');
  auto it = result.begin();

  it = std::copy(action.begin(), action.end(), it);
  it++;

  // 1/3 the screen width for the item, at most.
  int item_space = columns / 3;
  if (item_utf8.total_width() > item_space) {
    constexpr std::string_view kEllipses = "...";
    it = std::copy(kEllipses.begin(), kEllipses.end(), it);

    // Figure out how many UTF-8 chars to print.
    int current_width = 0;
    auto info_it = item_utf8.characters().crbegin();
    for (; info_it != item_utf8.characters().crend(); info_it++) {
      if (current_width + info_it->width > item_space - kEllipses.size()) {
        info_it--;
        break;
      }

      current_width += info_it->width;
    }

    it = std::copy(item.begin() + info_it->index, item.end(), it);
  } else {
    it = std::copy(item.begin(), item.end(), it);
    it += item_space - item_utf8.total_width();
  }

  it++;
  *it++ = '[';

  // Action character + space + item + space + opening bracket.
  int cols_taken = 1 + 1 + item_space + 1 + 1;
  // Closing bracket + space + max percent size (XXX.X%).
  constexpr int kExtraLength = 1 + 1 + 6;

  int remaining_cols = columns - cols_taken - kExtraLength;
  int filled_cols = remaining_cols * progress;

  for (int i = 0; i < remaining_cols; i++) {
    *it++ = i < filled_cols ? '=' : '-';
  }

  *it++ = ']';
  *it++ = ' ';

  int progress_factor = 1000;
  int progress_scaled = progress * progress_factor;
  for (int i = 0; i < 4; i++) {
    if (i == 3) {
      *it++ = '.';
    }

    int digit = progress_scaled / progress_factor;
    if (digit == 0 && i < 2) {
      *it++ = ' ';
    } else {
      *it++ = digit + '0';
    }
    progress_scaled -= digit * progress_factor;
    progress_factor /= 10;
  }

  *it++ = '%';

  result.resize(it - result.begin());
  return result;
}

}  // namespace netpull
