// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_

#include "ash/session/session_state_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
class SessionStateObserver;
}  // namespace ash

class SessionStateDelegate : public ash::SessionStateDelegate {
 public:
  SessionStateDelegate();
  virtual ~SessionStateDelegate();

  // ash::SessionStateDelegate:
  virtual content::BrowserContext* GetBrowserContextByIndex(
      ash::MultiProfileIndex index) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextForWindow(
      aura::Window* window) OVERRIDE;
  virtual int GetMaximumNumberOfLoggedInUsers() const OVERRIDE;
  virtual int NumberOfLoggedInUsers() const OVERRIDE;
  virtual bool IsActiveUserSessionStarted() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual bool ShouldLockScreenBeforeSuspending() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;
  virtual bool IsUserSessionBlocked() const OVERRIDE;
  virtual SessionState GetSessionState() const OVERRIDE;
  virtual const user_manager::UserInfo* GetUserInfo(
      ash::MultiProfileIndex index) const OVERRIDE;
  virtual const user_manager::UserInfo* GetUserInfo(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ShouldShowAvatar(aura::Window* window) const OVERRIDE;
  virtual void SwitchActiveUser(const std::string& user_id) OVERRIDE;
  virtual void CycleActiveUser(CycleUser cycle_user) OVERRIDE;
  virtual void AddSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;
  virtual void RemoveSessionStateObserver(
      ash::SessionStateObserver* observer) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_SESSION_STATE_DELEGATE_VIEWS_H_
