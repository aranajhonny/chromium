// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user_type.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

namespace {

// User dictionary keys.
const char kKeyUsername[] = "username";
const char kKeyDisplayName[] = "displayName";
const char kKeyEmailAddress[] = "emailAddress";
const char kKeyEnterpriseDomain[] = "enterpriseDomain";
const char kKeyPublicAccount[] = "publicAccount";
const char kKeySupervisedUser[] = "locallyManagedUser";
const char kKeySignedIn[] = "signedIn";
const char kKeyCanRemove[] = "canRemove";
const char kKeyIsOwner[] = "isOwner";
const char kKeyInitialAuthType[] = "initialAuthType";
const char kKeyMultiProfilesAllowed[] = "isMultiProfilesAllowed";
const char kKeyMultiProfilesPolicy[] = "multiProfilesPolicy";

// Max number of users to show.
// Please keep synced with one in signin_userlist_unittest.cc.
const size_t kMaxUsers = 18;

const int kPasswordClearTimeoutSec = 60;

}  // namespace

UserSelectionScreen::UserSelectionScreen() : handler_(NULL) {
}

UserSelectionScreen::~UserSelectionScreen() {
  wm::UserActivityDetector* activity_detector =
      ash::Shell::GetInstance()->user_activity_detector();
  if (activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

// static
void UserSelectionScreen::FillUserDictionary(
    User* user,
    bool is_owner,
    bool is_signin_to_add,
    ScreenlockBridge::LockHandler::AuthType auth_type,
    base::DictionaryValue* user_dict) {
  const std::string& user_id = user->email();
  const bool is_public_account =
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;
  const bool is_supervised_user =
      user->GetType() == user_manager::USER_TYPE_SUPERVISED;

  user_dict->SetString(kKeyUsername, user_id);
  user_dict->SetString(kKeyEmailAddress, user->display_email());
  user_dict->SetString(kKeyDisplayName, user->GetDisplayName());
  user_dict->SetBoolean(kKeyPublicAccount, is_public_account);
  user_dict->SetBoolean(kKeySupervisedUser, is_supervised_user);
  user_dict->SetInteger(kKeyInitialAuthType, auth_type);
  user_dict->SetBoolean(kKeySignedIn, user->is_logged_in());
  user_dict->SetBoolean(kKeyIsOwner, is_owner);

  // Fill in multi-profiles related fields.
  if (is_signin_to_add) {
    MultiProfileUserController* multi_profile_user_controller =
        UserManager::Get()->GetMultiProfileUserController();
    std::string behavior =
        multi_profile_user_controller->GetCachedValue(user_id);
    user_dict->SetBoolean(kKeyMultiProfilesAllowed,
                          multi_profile_user_controller->IsUserAllowedInSession(
                              user_id) == MultiProfileUserController::ALLOWED);
    user_dict->SetString(kKeyMultiProfilesPolicy, behavior);
  } else {
    user_dict->SetBoolean(kKeyMultiProfilesAllowed, true);
  }

  if (is_public_account) {
    policy::BrowserPolicyConnectorChromeOS* policy_connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();

    if (policy_connector->IsEnterpriseManaged()) {
      user_dict->SetString(kKeyEnterpriseDomain,
                           policy_connector->GetEnterpriseDomain());
    }
  }
}

// static
bool UserSelectionScreen::ShouldForceOnlineSignIn(const User* user) {
  // Public sessions are always allowed to log in offline.
  // Supervised user are allowed to log in offline if their OAuth token status
  // is unknown or valid.
  // For all other users, force online sign in if:
  // * The flag to force online sign-in is set for the user.
  // * The user's OAuth token is invalid.
  // * The user's OAuth token status is unknown (except supervised users,
  //   see above).
  if (user->is_logged_in())
    return false;

  const User::OAuthTokenStatus token_status = user->oauth_token_status();
  const bool is_supervised_user =
      user->GetType() == user_manager::USER_TYPE_SUPERVISED;
  const bool is_public_session =
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT;

  if (is_supervised_user &&
      token_status == User::OAUTH_TOKEN_STATUS_UNKNOWN) {
    return false;
  }

  if (is_public_session)
    return false;

  return user->force_online_signin() ||
         (token_status == User::OAUTH2_TOKEN_STATUS_INVALID) ||
         (token_status == User::OAUTH_TOKEN_STATUS_UNKNOWN);
}

void UserSelectionScreen::SetHandler(LoginDisplayWebUIHandler* handler) {
  handler_ = handler;
}

void UserSelectionScreen::Init(const UserList& users, bool show_guest) {
  users_ = users;
  show_guest_ = show_guest;

  wm::UserActivityDetector* activity_detector =
      ash::Shell::GetInstance()->user_activity_detector();
  if (!activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

void UserSelectionScreen::OnBeforeUserRemoved(const std::string& username) {
  for (UserList::iterator it = users_.begin(); it != users_.end(); ++it) {
    if ((*it)->email() == username) {
      users_.erase(it);
      break;
    }
  }
}

void UserSelectionScreen::OnUserRemoved(const std::string& username) {
  if (!handler_)
    return;

  handler_->OnUserRemoved(username);
}

void UserSelectionScreen::OnUserImageChanged(const User& user) {
  if (!handler_)
    return;
  handler_->OnUserImageChanged(user);
  // TODO(antrim) : updateUserImage(user.email())
}

const UserList& UserSelectionScreen::GetUsers() const {
  return users_;
}

void UserSelectionScreen::OnPasswordClearTimerExpired() {
  if (handler_)
    handler_->ClearUserPodPassword();
}

void UserSelectionScreen::OnUserActivity(const ui::Event* event) {
  if (!password_clear_timer_.IsRunning()) {
    password_clear_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kPasswordClearTimeoutSec),
        this,
        &UserSelectionScreen::OnPasswordClearTimerExpired);
  }
  password_clear_timer_.Reset();
}

// static
const UserList UserSelectionScreen::PrepareUserListForSending(
    const UserList& users,
    std::string owner,
    bool is_signin_to_add) {

  UserList users_to_send;
  bool has_owner = owner.size() > 0;
  size_t max_non_owner_users = has_owner ? kMaxUsers - 1 : kMaxUsers;
  size_t non_owner_count = 0;

  for (UserList::const_iterator it = users.begin(); it != users.end(); ++it) {
    const std::string& user_id = (*it)->email();
    bool is_owner = (user_id == owner);
    bool is_public_account =
        ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT);

    if ((is_public_account && !is_signin_to_add) || is_owner ||
        (!is_public_account && non_owner_count < max_non_owner_users)) {

      if (!is_owner)
        ++non_owner_count;
      if (is_owner && users_to_send.size() > kMaxUsers) {
        // Owner is always in the list.
        users_to_send.insert(users_to_send.begin() + (kMaxUsers - 1), *it);
        while (users_to_send.size() > kMaxUsers)
          users_to_send.erase(users_to_send.begin() + kMaxUsers);
      } else if (users_to_send.size() < kMaxUsers) {
        users_to_send.push_back(*it);
      }
    }
  }
  return users_to_send;
}

void UserSelectionScreen::SendUserList() {
  base::ListValue users_list;
  const UserList& users = GetUsers();

  // TODO(nkostylev): Move to a separate method in UserManager.
  // http://crbug.com/230852
  bool single_user = users.size() == 1;
  bool is_signin_to_add = LoginDisplayHostImpl::default_host() &&
                          UserManager::Get()->IsUserLoggedIn();
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  bool is_enterprise_managed = connector->IsEnterpriseManaged();

  const UserList users_to_send = PrepareUserListForSending(users,
                                                           owner,
                                                           is_signin_to_add);

  user_auth_type_map_.clear();

  for (UserList::const_iterator it = users_to_send.begin();
       it != users_to_send.end();
       ++it) {
    const std::string& user_id = (*it)->email();
    bool is_owner = (user_id == owner);
    const bool is_public_account =
        ((*it)->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT);
    const ScreenlockBridge::LockHandler::AuthType initial_auth_type =
        is_public_account
            ? ScreenlockBridge::LockHandler::EXPAND_THEN_USER_CLICK
            : (ShouldForceOnlineSignIn(*it)
                   ? ScreenlockBridge::LockHandler::ONLINE_SIGN_IN
                   : ScreenlockBridge::LockHandler::OFFLINE_PASSWORD);
    user_auth_type_map_[user_id] = initial_auth_type;

    base::DictionaryValue* user_dict = new base::DictionaryValue();
    FillUserDictionary(
        *it, is_owner, is_signin_to_add, initial_auth_type, user_dict);
    bool signed_in = (*it)->is_logged_in();
    // Single user check here is necessary because owner info might not be
    // available when running into login screen on first boot.
    // See http://crosbug.com/12723
    bool can_remove_user =
        ((!single_user || is_enterprise_managed) && !user_id.empty() &&
         !is_owner && !is_public_account && !signed_in && !is_signin_to_add);
    user_dict->SetBoolean(kKeyCanRemove, can_remove_user);
    users_list.Append(user_dict);
  }

  handler_->LoadUsers(users_list, show_guest_);
}

void UserSelectionScreen::HandleGetUsers() {
  SendUserList();
}

void UserSelectionScreen::SetAuthType(
    const std::string& username,
    ScreenlockBridge::LockHandler::AuthType auth_type) {
  user_auth_type_map_[username] = auth_type;
}

ScreenlockBridge::LockHandler::AuthType UserSelectionScreen::GetAuthType(
    const std::string& username) const {
  if (user_auth_type_map_.find(username) == user_auth_type_map_.end())
    return ScreenlockBridge::LockHandler::OFFLINE_PASSWORD;
  return user_auth_type_map_.find(username)->second;
}

}  // namespace chromeos
