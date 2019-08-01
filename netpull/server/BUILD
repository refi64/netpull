# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cc_binary(
  name = 'server',
  srcs = [
    'fast_crawler.h',
    'fast_crawler.cc',
    'netpull_server.cc',
    'path.h',
    'path.cc',
  ],
  deps = [
    '//netpull:common',
    '//netpull:cc_proto',
    '//netpull/network',
    '//netpull/parallel',
    '@com_google_absl//absl/container:flat_hash_map',
    '@com_google_absl//absl/debugging:failure_signal_handler',
    '@com_google_absl//absl/debugging:symbolize',
    '@com_google_absl//absl/flags:flag',
    '@com_google_absl//absl/flags:parse',
    '@com_google_absl//absl/time',
    '@com_google_absl//absl/strings',
  ],
  visibility = ['//visibility:public'],
)
