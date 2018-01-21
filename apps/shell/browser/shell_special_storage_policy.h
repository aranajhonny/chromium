// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_SPECIAL_STORAGE_POLICY_H_
#define APPS_SHELL_BROWSER_SHELL_SPECIAL_STORAGE_POLICY_H_

#include "webkit/browser/quota/special_storage_policy.h"

namespace apps {

// A simple storage policy for app_shell which does not limit storage
// capabilities and aims to be as permissive as possible.
class ShellSpecialStoragePolicy : public quota::SpecialStoragePolicy {
 public:
  ShellSpecialStoragePolicy();

  // quota::SpecialStoragePolicy implementation.
  virtual bool IsStorageProtected(const GURL& origin) OVERRIDE;
  virtual bool IsStorageUnlimited(const GURL& origin) OVERRIDE;
  virtual bool IsStorageSessionOnly(const GURL& origin) OVERRIDE;
  virtual bool CanQueryDiskSize(const GURL& origin) OVERRIDE;
  virtual bool IsFileHandler(const std::string& extension_id) OVERRIDE;
  virtual bool HasIsolatedStorage(const GURL& origin) OVERRIDE;
  virtual bool HasSessionOnlyOrigins() OVERRIDE;

 protected:
  virtual ~ShellSpecialStoragePolicy();
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_SPECIAL_STORAGE_POLICY_H
