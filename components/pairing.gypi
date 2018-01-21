# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'pairing',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
      ],
      'sources': [
        'pairing/fake_controller_pairing_controller.cc',
        'pairing/fake_controller_pairing_controller.h',
        'pairing/fake_host_pairing_controller.cc',
        'pairing/fake_host_pairing_controller.h',
        'pairing/controller_pairing_controller.cc',
        'pairing/controller_pairing_controller.h',
        'pairing/host_pairing_controller.cc',
        'pairing/host_pairing_controller.h',
      ],
    },
  ],
}
