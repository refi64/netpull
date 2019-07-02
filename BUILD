# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

proto_library(
  name = 'netpull_proto',
  srcs = ['netpull/netpull.proto'],
  deps = ['@com_google_protobuf//:timestamp_proto'],
)

cc_proto_library(
  name = 'netpull_cc_proto',
  deps = [':netpull_proto'],
)

cc_library(
  name = 'netpull_common',
  srcs = [
    'netpull/console.h',
    'netpull/console.cc',
    'netpull/crypto.h',
    'netpull/crypto.cc',
    'netpull/scoped_resource.h',
  ],
  deps = [
    '@boringssl//:crypto',
    '@com_google_absl//absl/strings:str_format',
    '@com_google_absl//absl/synchronization',
  ],
)

cc_library(
  name = 'netpull_network',
  srcs = [
    'netpull/network/ip.h',
    'netpull/network/ip.cc',
    'netpull/network/socket.h',
    'netpull/network/socket.cc',
  ],
  deps = [
    ':netpull_common',
    '@com_google_protobuf//:protobuf',
  ],
)

cc_library(
  name = 'netpull_parallel',
  srcs = [
    'netpull/parallel/guarded_map.h',
    'netpull/parallel/guarded_set.h',
    'netpull/parallel/worker_pool.h',
    'netpull/parallel/worker_pool.cc',
  ],
  deps = [
    ':netpull_common',
    '@com_google_absl//absl/container:flat_hash_map',
    '@com_google_absl//absl/container:flat_hash_set',
    '@com_google_absl//absl/synchronization',
  ],
)

cc_binary(
  name = 'netpull_server',
  srcs = [
    'netpull/server/fast_crawler.h',
    'netpull/server/fast_crawler.cc',
    'netpull/server/main.cc',
    'netpull/server/path.h',
    'netpull/server/path.cc',
  ],
  deps = [
    ':netpull_common',
    ':netpull_cc_proto',
    ':netpull_network',
    ':netpull_parallel',
    '@com_google_absl//absl/flags:flag',
    '@com_google_absl//absl/flags:parse',
    '@com_google_absl//absl/strings',
  ],
)

cc_binary(
  name = 'netpull_client',
  srcs = [
    'netpull/client/main.cc',
    'netpull/client/progress_builder.h',
    'netpull/client/progress_builder.cc',
    'netpull/client/utf8.h',
    'netpull/client/utf8.cc',
  ],
  deps = [
    ':netpull_common',
    ':netpull_cc_proto',
    ':netpull_network',
    ':netpull_parallel',
    '@com_google_absl//absl/flags:flag',
    '@com_google_absl//absl/flags:parse',
    '@com_google_absl//absl/synchronization',
    '@com_google_absl//absl/time',
    '@wcwidth//:wcwidth',
  ],
)
