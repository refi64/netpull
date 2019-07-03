# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cc_binary(
  name = 'client',
  srcs = [
    'main.cc',
    'progress_builder.h',
    'progress_builder.cc',
    'utf8.h',
    'utf8.cc',
  ],
  deps = [
    '//netpull:common',
    '//netpull:cc_proto',
    '//netpull/network',
    '//netpull/parallel',
    '@com_google_absl//absl/debugging:failure_signal_handler',
    '@com_google_absl//absl/debugging:symbolize',
    '@com_google_absl//absl/flags:flag',
    '@com_google_absl//absl/flags:parse',
    '@com_google_absl//absl/strings',
    '@com_google_absl//absl/synchronization',
    '@com_google_absl//absl/time',
    '@wcwidth//:wcwidth',
  ],
  visibility = ['//visibility:public'],
)
