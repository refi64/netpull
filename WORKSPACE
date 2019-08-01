# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository', 'new_git_repository')

git_repository(
  name = 'bazel_skylib',
  remote = 'https://github.com/bazelbuild/bazel-skylib',
  # tag = '0.9.0',
  commit = '2b38b2f8bd4b8603d610cfc651fcbb299498147f',
  shallow_since = '1562957722 -0400',
)

git_repository(
  name = 'boringssl',
  remote = 'https://github.com/google/boringssl',
  # branch = 'chromium-stable-with-bazel',
  commit = '420fdf45d1a118f2cb723497f4110b7a4fb0d854',
  shallow_since = '1560187604 +0000',
)

git_repository(
  name = 'com_google_protobuf',
  remote = 'https://github.com/protocolbuffers/protobuf',
  # tag = 'v3.9.0',
  commit = '6a59a2ad1f61d9696092f79b6d74368b4d7970a3',
  shallow_since = '1562856725 -0700',
)

git_repository(
  name = 'com_google_protobuf_cc',
  remote = 'https://github.com/protocolbuffers/protobuf',
  # tag = 'v3.9.0',
  commit = '6a59a2ad1f61d9696092f79b6d74368b4d7970a3',
  shallow_since = '1562856725 -0700',
)

git_repository(
  name = 'com_google_absl',
  remote = 'https://github.com/abseil/abseil-cpp',
  # branch = 'master',
  commit = '14550beb3b7b97195e483fb74b5efb906395c31e',
  shallow_since = '1564603675 -0400' ,
)

new_git_repository(
  name = 'wcwidth',
  remote = 'https://github.com/termux/wcwidth',
  commit = '096b8e6473c468be71d1241a58c025cad29a8043',
  shallow_since = '1477268057 +0200',
  build_file = 'BUILD.wcwidth',
)

load('@com_google_protobuf//:protobuf_deps.bzl', 'protobuf_deps')
protobuf_deps()
