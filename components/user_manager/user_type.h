// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_TYPE_H_
#define COMPONENTS_USER_MANAGER_USER_TYPE_H_

#include "components/user_manager/user_manager_export.h"

namespace user_manager {

// The user type. Used in a histogram; do not modify existing types.
USER_MANAGER_EXPORT typedef enum {
  // Regular user, has a user name and password.
  USER_TYPE_REGULAR = 0,
  // Guest user, logs in without authentication.
  USER_TYPE_GUEST = 1,
  // Retail mode user, logs in without authentication. This is a special user
  // type used in retail mode only.
  USER_TYPE_RETAIL_MODE = 2,
  // Public account user, logs in without authentication. Available only if
  // enabled through policy.
  USER_TYPE_PUBLIC_ACCOUNT = 3,
  // Supervised user, logs in only with local authentication.
  USER_TYPE_SUPERVISED = 4,
  // Kiosk app robot, logs in without authentication.
  USER_TYPE_KIOSK_APP = 5,
  // Maximum histogram value.
  NUM_USER_TYPES = 6
} UserType;

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_TYPE_H_
