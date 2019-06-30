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
    'netpull/network.h',
    'netpull/network.cc',
    'netpull/parallel.h',
    'netpull/parallel.cc',
    'netpull/scoped_resource.h',
  ],
  deps = [
    '@com_google_absl//absl/container:flat_hash_map',
    '@com_google_absl//absl/strings:str_format',
    '@com_google_absl//absl/synchronization',
    '@com_google_protobuf//:protobuf',
  ],
)

cc_binary(
  name = 'netpull_server',
  srcs = [
    'netpull/server/filesystem.h',
    'netpull/server/filesystem.cc',
    'netpull/server/main.cc',
  ],
  deps = [
    ':netpull_common',
    ':netpull_cc_proto',
    '@boringssl//:crypto',
    '@com_google_absl//absl/flags:flag',
    '@com_google_absl//absl/flags:parse',
    '@com_google_absl//absl/strings',
  ],
)

cc_binary(
  name = 'netpull_client',
  srcs = [
    'netpull/client/main.cc',
  ],
  deps = [
    ':netpull_common',
    ':netpull_cc_proto',
    '@com_google_absl//absl/flags:flag',
    '@com_google_absl//absl/flags:parse',
  ],
)
