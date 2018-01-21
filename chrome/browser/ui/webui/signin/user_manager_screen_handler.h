// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/gaia_auth_consumer.h"

class GaiaAuthFetcher;

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

class UserManagerScreenHandler : public content::WebUIMessageHandler,
                                 public ScreenlockBridge::LockHandler,
                                 public GaiaAuthConsumer {
 public:
  UserManagerScreenHandler();
  virtual ~UserManagerScreenHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

  // ScreenlockBridge::LockHandler implementation.
  virtual void ShowBannerMessage(const std::string& message) OVERRIDE;
  virtual void ShowUserPodCustomIcon(const std::string& user_email,
                                     const gfx::Image& icon) OVERRIDE;
  virtual void HideUserPodCustomIcon(const std::string& user_email) OVERRIDE;
  virtual void EnableInput() OVERRIDE;
  virtual void SetAuthType(const std::string& user_email,
                           ScreenlockBridge::LockHandler::AuthType auth_type,
                           const std::string& auth_value) OVERRIDE;
  virtual ScreenlockBridge::LockHandler::AuthType GetAuthType(
      const std::string& user_email) const OVERRIDE;
  virtual void Unlock(const std::string& user_email) OVERRIDE;

 private:
  // An observer for any changes to Profiles in the ProfileInfoCache so that
  // all the visible user manager screens can be updated.
  class ProfileUpdateObserver;

  void HandleInitialize(const base::ListValue* args);
  void HandleAddUser(const base::ListValue* args);
  void HandleAuthenticatedLaunchUser(const base::ListValue* args);
  void HandleLaunchGuest(const base::ListValue* args);
  void HandleLaunchUser(const base::ListValue* args);
  void HandleRemoveUser(const base::ListValue* args);
  void HandleAttemptUnlock(const base::ListValue* args);

  // Handle GAIA auth results.
  virtual void OnClientLoginSuccess(const ClientLoginResult& result) OVERRIDE;
  virtual void OnClientLoginFailure(const GoogleServiceAuthError& error)
      OVERRIDE;

  // Sends user list to account chooser.
  void SendUserList();

  // Pass success/failure information back to the web page.
  void ReportAuthenticationResult(bool success,
                                  ProfileMetrics::ProfileAuth metric);

  // Observes the ProfileInfoCache and gets notified when a profile has been
  // modified, so that the displayed user pods can be updated.
  scoped_ptr<ProfileUpdateObserver> profileInfoCacheObserver_;

  // The host desktop type this user manager belongs to.
  chrome::HostDesktopType desktop_type_;

  // Authenticator used when local-auth fails.
  scoped_ptr<GaiaAuthFetcher> client_login_;

  // The index of the profile currently being authenticated.
  size_t authenticating_profile_index_;

  // Login password, held during on-line auth for saving later if correct.
  std::string password_attempt_;

  typedef std::map<std::string, ScreenlockBridge::LockHandler::AuthType>
      UserAuthTypeMap;
  UserAuthTypeMap user_auth_type_map_;

  DISALLOW_COPY_AND_ASSIGN(UserManagerScreenHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_USER_MANAGER_SCREEN_HANDLER_H_
