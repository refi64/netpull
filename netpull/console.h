/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <errno.h>

#include <list>
#include <memory>

#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"

namespace netpull {

enum class LogLevel { kVerbose, kInfo, kWarning, kError, kErrno };

void EnableVerboseLogging();

namespace console_internal {
  void LogGenericString(LogLevel level, std::string str);

  template <typename... Args>
  void LogGeneric(LogLevel level, const absl::FormatSpec<Args...>& fmt, const Args&... args) {
    int errno_save = errno;
    std::string formatted = absl::StrFormat(fmt, args...);
    errno = errno_save;
    LogGenericString(level, std::move(formatted));
  }
}

#define DEFINE_LOG_ALIAS(level) \
  template <typename... Args> \
  void Log##level(const absl::FormatSpec<Args...>& fmt, const Args&... args) { \
    console_internal::LogGeneric(LogLevel::k##level, fmt, args...); \
  } \
  __attribute__((unused)) static void Log##level(std::string str) { \
    console_internal::LogGenericString(LogLevel::k##level, std::move(str)); \
  }

DEFINE_LOG_ALIAS(Verbose)
DEFINE_LOG_ALIAS(Info)
DEFINE_LOG_ALIAS(Warning)
DEFINE_LOG_ALIAS(Error)
DEFINE_LOG_ALIAS(Errno)

namespace ansi {
  constexpr std::string_view
    kFgBright{"\033[1m"},
    kFgBlack{"\033[30m"},
    kFgRed{"\033[31m"},
    kFgGreen{"\033[32m"},
    kFgYellow{"\033[33m"},
    kFgBlue{"\033[34m"},
    kFgMagenta{"\033[35m"},
    kFgCyan{"\033[36m"},
    kFgWhite{"\033[37m"},
    kBgBlack{"\033[40m"},
    kBgRed{"\033[41m"},
    kBgGreen{"\033[42m"},
    kBgYellow{"\033[43m"},
    kBgBlue{"\033[44m"},
    kBgMagenta{"\033[45m"},
    kBgCyan{"\033[46m"},
    kBgWhite{"\033[47m"},
    kBold{"\033[1m"},
    kReset{"\033[0m"},

    kMoveUp{"\033[A"},
    kClearToEos{"\033[J"};
}

class ConsoleLine {
public:
  ConsoleLine(const ConsoleLine& other)=delete;
  ConsoleLine(ConsoleLine&& other)=delete;
  ~ConsoleLine();

  static std::unique_ptr<ConsoleLine> Claim();

  // XXX: This is kinda ugly but I don't want to give LogGenericString all private access
  // via friend.
  static void InternalUnsafeEraseOldLines();
  static void InternalUnsafeDrawAll();
  static void DrawAll();

  static size_t count() { return lines.size(); }

  void Update(std::string line);

private:
  ConsoleLine() {}

  std::list<ConsoleLine*>::iterator it;
  std::string content;

  static std::list<ConsoleLine*> lines;
  static int diff;
};

}
