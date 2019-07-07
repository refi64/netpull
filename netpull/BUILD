# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

proto_library(
  name = 'proto',
  srcs = ['netpull.proto'],
  deps = ['@com_google_protobuf//:timestamp_proto'],
)

cc_proto_library(
  name = 'cc_proto',
  deps = [':proto'],
  visibility = ['//visibility:public'],
)

cc_library(
  name = 'common',
  srcs = [
    'assert.h',
    'console.h',
    'console.cc',
    'crypto.h',
    'crypto.cc',
    'scoped_resource.h',
  ],
  deps = [
    '@boringssl//:crypto',
    '@com_google_absl//absl/strings:str_format',
    '@com_google_absl//absl/synchronization',
  ],
  visibility = ['//visibility:public'],
)
