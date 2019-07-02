# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cc_library(
  name = 'parallel',
  srcs = [
    'guarded_map.h',
    'guarded_set.h',
    'worker_pool.h',
    'worker_pool.cc',
  ],
  deps = [
    '//netpull:common',
    '@com_google_absl//absl/container:flat_hash_map',
    '@com_google_absl//absl/container:flat_hash_set',
    '@com_google_absl//absl/synchronization',
  ],
  visibility = ['//visibility:public'],
)
