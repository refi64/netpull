/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include "netpull/console.h"

#define NETPULL_ASSERT_STRINGIFY2(x) #x
#define NETPULL_ASSERT_STRINGIFY(x) NETPULL_ASSERT_STRINGIFY2(x)

// Asserts the given condition, printing the given message if it fails.
#define NETPULL_ASSERT(cond, ...) \
  do { \
    if (!(cond)) { \
      ::netpull::LogError("Assertion [" #cond "] failed (" \
        __FILE__ ":" NETPULL_ASSERT_STRINGIFY(__LINE__) "): " __VA_ARGS__); \
      abort(); \
    } \
  } while (0)
