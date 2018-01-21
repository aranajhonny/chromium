// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_notification_manager_factory.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveNotificationManagerFactoryLoginScreenBrowserTest
    : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kLoginManager);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }
};

// Verify that no DriveNotificationManager is instantiated for the sign-in
// profile on the login screen.
IN_PROC_BROWSER_TEST_F(DriveNotificationManagerFactoryLoginScreenBrowserTest,
                       NoDriveNotificationManager) {
  Profile* signin_profile =
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(DriveNotificationManagerFactory::FindForBrowserContext(
      signin_profile));
}

class DriveNotificationManagerFactoryGuestBrowserTest
    : public InProcessBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    chromeos::login::kGuestUserName);
  }
};

// Verify that no DriveNotificationManager is instantiated for the sign-in
// profile or the guest profile while a guest session is in progress.
IN_PROC_BROWSER_TEST_F(DriveNotificationManagerFactoryGuestBrowserTest,
                       NoDriveNotificationManager) {
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
  Profile* guest_profile = chromeos::ProfileHelper::Get()
                               ->GetProfileByUser(user_manager->GetActiveUser())
                               ->GetOriginalProfile();
  Profile* signin_profile =
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(DriveNotificationManagerFactory::FindForBrowserContext(
      guest_profile));
  EXPECT_FALSE(DriveNotificationManagerFactory::FindForBrowserContext(
      signin_profile));
}

}  // namespace drive
