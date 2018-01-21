// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/touchscreen_device_manager_x11.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "ui/gfx/x/x11_types.h"

namespace {

// We consider the touchscreen to be internal if it is an I2c device.
// With the device id, we can query X to get the device's dev input
// node eventXXX. Then we search all the dev input nodes registered
// by I2C devices to see if we can find eventXXX.
bool IsTouchscreenInternal(XDisplay* dpy, int device_id) {
  using base::FileEnumerator;
  using base::FilePath;

  if (!base::SysInfo::IsRunningOnChromeOS())
    return false;

  // Input device has a property "Device Node" pointing to its dev input node,
  // e.g.   Device Node (250): "/dev/input/event8"
  Atom device_node = XInternAtom(dpy, "Device Node", False);
  if (device_node == None)
    return false;

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char* data;
  XDevice* dev = XOpenDevice(dpy, device_id);
  if (!dev)
    return false;

  if (XGetDeviceProperty(dpy, dev, device_node, 0, 1000, False,
                         AnyPropertyType, &actual_type, &actual_format,
                         &nitems, &bytes_after, &data) != Success) {
    XCloseDevice(dpy, dev);
    return false;
  }
  base::FilePath dev_node_path(reinterpret_cast<char*>(data));
  XFree(data);
  XCloseDevice(dpy, dev);

  std::string event_node = dev_node_path.BaseName().value();
  if (event_node.empty() ||
      !StartsWithASCII(event_node, "event", false)) {
    return false;
  }

  // Extract id "XXX" from "eventXXX"
  std::string event_node_id = event_node.substr(5);

  // I2C input device registers its dev input node at
  // /sys/bus/i2c/devices/*/input/inputXXX/eventXXX
  FileEnumerator i2c_enum(FilePath(FILE_PATH_LITERAL("/sys/bus/i2c/devices/")),
                          false,
                          base::FileEnumerator::DIRECTORIES);
  for (FilePath i2c_name = i2c_enum.Next();
       !i2c_name.empty();
       i2c_name = i2c_enum.Next()) {
    FileEnumerator input_enum(i2c_name.Append(FILE_PATH_LITERAL("input")),
                              false,
                              base::FileEnumerator::DIRECTORIES,
                              FILE_PATH_LITERAL("input*"));
    for (base::FilePath input = input_enum.Next();
         !input.empty();
         input = input_enum.Next()) {
      if (input.BaseName().value().substr(5) == event_node_id)
        return true;
    }
  }

  return false;
}

}  // namespace

namespace ui {

TouchscreenDeviceManagerX11::TouchscreenDeviceManagerX11()
    : display_(gfx::GetXDisplay()) {}

TouchscreenDeviceManagerX11::~TouchscreenDeviceManagerX11() {}

std::vector<TouchscreenDevice> TouchscreenDeviceManagerX11::GetDevices() {
  std::vector<TouchscreenDevice> devices;
  int num_devices = 0;
  Atom valuator_x = XInternAtom(display_, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display_, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return devices;

  std::set<int> no_match_touchscreen;
  XIDeviceInfo* info = XIQueryDevice(display_, XIAllDevices, &num_devices);
  for (int i = 0; i < num_devices; i++) {
    if (!info[i].enabled || info[i].use != XIFloatingSlave)
      continue;  // Assume all touchscreens are floating slaves

    double width = -1.0;
    double height = -1.0;
    bool is_direct_touch = false;

    for (int j = 0; j < info[i].num_classes; j++) {
      XIAnyClassInfo* class_info = info[i].classes[j];

      if (class_info->type == XIValuatorClass) {
        XIValuatorClassInfo* valuator_info =
            reinterpret_cast<XIValuatorClassInfo*>(class_info);

        if (valuator_x == valuator_info->label) {
          // Ignore X axis valuator with unexpected properties
          if (valuator_info->number == 0 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            width = valuator_info->max;
          }
        } else if (valuator_y == valuator_info->label) {
          // Ignore Y axis valuator with unexpected properties
          if (valuator_info->number == 1 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            height = valuator_info->max;
          }
        }
      }
#if defined(USE_XI2_MT)
      if (class_info->type == XITouchClass) {
        XITouchClassInfo* touch_info =
            reinterpret_cast<XITouchClassInfo*>(class_info);
        is_direct_touch = touch_info->mode == XIDirectTouch;
      }
#endif
    }

    // Touchscreens should have absolute X and Y axes, and be direct touch
    // devices.
    if (width > 0.0 && height > 0.0 && is_direct_touch) {
      bool is_internal = IsTouchscreenInternal(display_, info[i].deviceid);
      devices.push_back(TouchscreenDevice(info[i].deviceid,
                                          gfx::Size(width, height),
                                          is_internal));
    }
  }

  XIFreeDeviceInfo(info);
  return devices;
}

}  // namespace ui
