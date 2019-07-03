# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

genrule(
  name = 'copies',
  outs = [
    'netpull_client',
    'netpull_server',
  ],
  srcs = [
    '//netpull/client',
    '//netpull/server',
  ],
  cmd = 'cp $(location //netpull/client) $(RULEDIR)/netpull_client &&\
         cp $(location //netpull/server) $(RULEDIR)/netpull_server',
)
