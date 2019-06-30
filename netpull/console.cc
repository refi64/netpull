/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "absl/synchronization/mutex.h"

#include "netpull/console.h"

#include <iostream>

namespace netpull {

static absl::Mutex g_mutex;
static bool g_verbose_logging_enabled = false;

void EnableVerboseLogging() {
  g_verbose_logging_enabled = true;
}

void console_internal::LogGenericString(LogLevel level, std::string str) {
  int errno_save = errno;
  absl::MutexLock lock(&g_mutex);

  switch (level) {
  case LogLevel::kVerbose:
    if (!g_verbose_logging_enabled) {
      return;
    }

    std::cout
      << ansi::kBold << ansi::kFgYellow << ansi::kBgBlack << "[VERBOSE] " << ansi::kReset
      << ansi::kFgYellow << ansi::kBgBlack << str << ansi::kReset << std::endl;
    break;
  case LogLevel::kInfo:
    std::cout << str << std::endl;
    break;
  case LogLevel::kWarning:
    std::cerr
      << ansi::kBold << ansi::kFgMagenta << "[WARNING] " << ansi::kReset
      << str << std::endl;
    break;
  case LogLevel::kErrno:
    [[fallthrough]];
  case LogLevel::kError:
    std::cerr
      << ansi::kBold << ansi::kFgRed << "[ERROR] " << ansi::kReset
      << str;

    if (level == LogLevel::kErrno) {
      std::cerr << ": " << strerror(errno_save) << " [errno " << errno << "]";
    }

    std::cerr << std::endl;
    break;
  }
}

}  // namespace netpull
