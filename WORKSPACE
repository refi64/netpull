# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

load('@bazel_tools//tools/build_defs/repo:git.bzl', 'git_repository')

git_repository(
  name = 'bazel_skylib',
  remote = 'https://github.com/bazelbuild/bazel-skylib',
  # tag = '0.8.0',
  commit = '3721d32c14d3639ff94320c780a60a6e658fb033',
  shallow_since = '1553102012 +0100',
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
  remote = 'https://github.com/google/protobuf',
  # tag = 'v3.8.0',
  commit = '09745575a923640154bcf307fba8aedff47f240a',
  shallow_since = '1558721209 -0700',
)

git_repository(
  name = 'com_google_protobuf_cc',
  remote = 'https://github.com/google/protobuf',
  # tag = 'v3.8.0',
  commit = '09745575a923640154bcf307fba8aedff47f240a',
  shallow_since = '1558721209 -0700',
)

git_repository(
  name = 'com_google_absl',
  remote = 'https://github.com/abseil/abseil-cpp',
  # branch = 'master',
  commit = 'd65e19dfcd8697076f68598c0131c6930cdcd74d',
  shallow_since = '1561428275 -0400',
)

load('@com_google_protobuf//:protobuf_deps.bzl', 'protobuf_deps')
protobuf_deps()
