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
  if (level == LogLevel::kVerbose && !g_verbose_logging_enabled) {
    return;
  }

  absl::MutexLock lock(&g_mutex);

  ConsoleLine::InternalEraseAll();

  switch (level) {
  case LogLevel::kVerbose:
    std::cout
      << ansi::kBold << ansi::kFgYellow << ansi::kBgBlack << "[VERBOSE] " << ansi::kReset
      << ansi::kFgYellow << ansi::kBgBlack << str << ansi::kReset << '\n';
    break;
  case LogLevel::kInfo:
    std::cout << str << '\n';
    break;
  case LogLevel::kWarning:
    std::cout
      << ansi::kBold << ansi::kFgMagenta << "[WARNING] " << ansi::kReset
      << str << '\n';
    break;
  case LogLevel::kErrno:
    [[fallthrough]];
  case LogLevel::kError:
    std::cout
      << ansi::kBold << ansi::kFgRed << "[ERROR] " << ansi::kReset
      << str;

    if (level == LogLevel::kErrno) {
      std::cout << ": " << strerror(errno_save) << " [errno " << errno << "]";
    }

    std::cout << '\n';
    break;
  }

  ConsoleLine::InternalDrawAll();
}

ConsoleLine::~ConsoleLine() {
  absl::MutexLock lock(&g_mutex);
  lines.erase(it);
}

std::unique_ptr<ConsoleLine> ConsoleLine::Claim() {
  auto line = new ConsoleLine;
  absl::MutexLock lock(&g_mutex);
  lines.push_back(line);
  line->it = --lines.end();
  return std::unique_ptr<ConsoleLine>(line);
}

void ConsoleLine::InternalEraseAll() {
  std::cout << '\r';

  if (!lines.empty()) {
    for (int i = 0; i < lines.size() - 1; i++) {
      std::cout << ansi::kMoveUp;
    }
  }

  std::cout << ansi::kClearToEos;
}

void ConsoleLine::InternalDrawAll() {
  bool first = true;

  for (ConsoleLine* line : lines) {
    if (!first) {
      std::cout << '\n';
    } else {
      first = false;
    }

    std::cout << line->content;
  }

  std::cout << std::flush;
}

void ConsoleLine::DrawAll() {
  absl::MutexLock lock(&g_mutex);
  InternalEraseAll();
  InternalDrawAll();
}

void ConsoleLine::Update(std::string line) {
  content = std::move(line);
}

std::list<ConsoleLine*> ConsoleLine::lines;

}  // namespace netpull
