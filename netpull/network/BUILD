# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cc_library(
  name = 'network',
  srcs = [
    'ip.h',
    'ip.cc',
    'socket.h',
    'socket.cc',
  ],
  deps = [
    '//netpull:common',
    '@com_google_protobuf//:protobuf',
  ],
  visibility = ['//visibility:public'],
)
