// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/profiles/profile_chooser_controller.h"

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"

const std::string kEmail = "user@gmail.com";
const std::string kSecondaryEmail = "user2@gmail.com";
const std::string kLoginToken = "oauth2_login_token";

class ProfileChooserControllerTest : public CocoaProfileTest {
 public:
  ProfileChooserControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser()->profile());

    TestingProfile::TestingFactories factories;
    factories.push_back(
        std::make_pair(ProfileOAuth2TokenServiceFactory::GetInstance(),
                       BuildFakeProfileOAuth2TokenService));
    testing_profile_manager()->
        CreateTestingProfile("test1", scoped_ptr<PrefServiceSyncable>(),
                             base::ASCIIToUTF16("Test 1"), 0, std::string(),
                             factories);
    testing_profile_manager()->
        CreateTestingProfile("test2", scoped_ptr<PrefServiceSyncable>(),
                             base::ASCIIToUTF16("Test 2"), 1, std::string(),
                             TestingProfile::TestingFactories());

    menu_ = new AvatarMenu(testing_profile_manager()->profile_info_cache(),
                           NULL, NULL);
    menu_->RebuildMenu();

    // There should be the default profile + two profiles we created.
    EXPECT_EQ(3U, menu_->GetNumberOfItems());
  }

  virtual void TearDown() OVERRIDE {
    [controller() close];
    controller_.reset();
    CocoaProfileTest::TearDown();
  }

  void StartProfileChooserController() {
    NSRect frame = [test_window() frame];
    NSPoint point = NSMakePoint(NSMidX(frame), NSMidY(frame));
    controller_.reset([[ProfileChooserController alloc]
        initWithBrowser:browser()
             anchoredAt:point
               withMode:profiles::BUBBLE_VIEW_MODE_PROFILE_CHOOSER
        withServiceType:signin::GAIA_SERVICE_TYPE_NONE]);
    [controller_ showWindow:nil];
  }

  void EnableNewAvatarMenuOnly() {
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNewAvatarMenu);
  }

  void EnableFastUserSwitching() {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kFastUserSwitching);
  }

  ProfileChooserController* controller() { return controller_; }
  AvatarMenu* menu() { return menu_; }

 private:
  base::scoped_nsobject<ProfileChooserController> controller_;

  // Weak; owned by |controller_|.
  AvatarMenu* menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileChooserControllerTest);
};

TEST_F(ProfileChooserControllerTest, InitialLayoutWithNewManagement) {
  switches::EnableNewProfileManagementForTesting(
      CommandLine::ForCurrentProcess());
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // Three profiles means we should have one active card, one separator and
  // one option buttons view.
  EXPECT_EQ(3U, [subviews count]);

  // For a local profile, there should be one button in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [buttonSubviews count]);
  NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showUserManager:), [button action]);
  EXPECT_EQ(controller(), [button target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  EXPECT_EQ(3U, [activeCardSubviews count]);

  // Profile icon.
  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // Profile name.
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSButton*>(activeProfileName) title]));

  // Profile links. This is a local profile, so there should be a signin button.
  NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [linksSubviews count]);
  NSButton* link = static_cast<NSButton*>([linksSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showInlineSigninPage:), [link action]);
  EXPECT_EQ(controller(), [link target]);
}

TEST_F(ProfileChooserControllerTest, InitialLayoutWithNewMenu) {
  EnableNewAvatarMenuOnly();
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // Three profiles means we should have one active card and a
  // fast user switcher which has two "other" profiles and 2 separators. In
  // this flow we also have the tutorial view.
  EXPECT_EQ(6U, [subviews count]);

  // There should be two "other profiles" items. The items are drawn from the
  // bottom up, so in the opposite order of those in the AvatarMenu.
  int profileIndex = 1;
  for (int i = 3; i >= 0; i -= 2) {
    // Each profile button has a separator.
    EXPECT_TRUE([[subviews objectAtIndex:i] isKindOfClass:[NSBox class]]);

    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i-1]);
    EXPECT_EQ(menu()->GetItemAt(profileIndex).name,
              base::SysNSStringToUTF16([button title]));
    EXPECT_EQ(profileIndex, [button tag]);
    EXPECT_EQ(@selector(switchToProfile:), [button action]);
    EXPECT_EQ(controller(), [button target]);
    profileIndex++;
  }

  // There should be the profile avatar, name and links container in the active
  // card view. The links displayed in the container are checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  EXPECT_EQ(3U, [activeCardSubviews count]);

  // Profile icon.
  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // Profile name.
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSButton*>(activeProfileName) title]));

  // Profile links. This is a local profile, so there should be a signin button.
  NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [linksSubviews count]);
  NSButton* link = static_cast<NSButton*>([linksSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showTabbedSigninPage:), [link action]);
  EXPECT_EQ(controller(), [link target]);

  // There is a tutorial view card at the top.
  EXPECT_TRUE([[subviews objectAtIndex:5] isKindOfClass:[NSView class]]);
}

TEST_F(ProfileChooserControllerTest, InitialLayoutWithFastUserSwitcher) {
  switches::EnableNewProfileManagementForTesting(
      CommandLine::ForCurrentProcess());
  EnableFastUserSwitching();
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // Three profiles means we should have one active card, two "other" profiles,
  // each with a separator, and one option buttons view.
  EXPECT_EQ(7U, [subviews count]);

  // For a local profile, there should be one button in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [buttonSubviews count]);
  NSButton* button = static_cast<NSButton*>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showUserManager:), [button action]);
  EXPECT_EQ(controller(), [button target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // There should be two "other profiles" items. The items are drawn from the
  // bottom up, so in the opposite order of those in the AvatarMenu.
  int profileIndex = 1;
  for (int i = 5; i >= 2; i -= 2) {
    // Each profile button has a separator.
    EXPECT_TRUE([[subviews objectAtIndex:i] isKindOfClass:[NSBox class]]);

    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i-1]);
    EXPECT_EQ(menu()->GetItemAt(profileIndex).name,
              base::SysNSStringToUTF16([button title]));
    EXPECT_EQ(profileIndex, [button tag]);
    EXPECT_EQ(@selector(switchToProfile:), [button action]);
    EXPECT_EQ(controller(), [button target]);
    profileIndex++;
  }

  // There should be the profile avatar, name and links container in the active
  // card view. These have been checked separately.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:6] subviews];
  EXPECT_EQ(3U, [activeCardSubviews count]);
}

TEST_F(ProfileChooserControllerTest, OtherProfilesSortedAlphabetically) {
  EnableNewAvatarMenuOnly();

  // Add two extra profiles, to make sure sorting is alphabetical and not
  // by order of creation.
  testing_profile_manager()->
      CreateTestingProfile("test3", scoped_ptr<PrefServiceSyncable>(),
                           base::ASCIIToUTF16("New Profile"), 1, std::string(),
                           TestingProfile::TestingFactories());
  testing_profile_manager()->
      CreateTestingProfile("test4", scoped_ptr<PrefServiceSyncable>(),
                           base::ASCIIToUTF16("Another Test"), 1, std::string(),
                           TestingProfile::TestingFactories());
  StartProfileChooserController();

  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSString* sortedNames[] = { @"Another Test",
                              @"New Profile",
                              @"Test 1",
                              @"Test 2" };
  // There are four "other" profiles, each with a button and a separator, an
  // active profile card, and a tutorial card.
  EXPECT_EQ(10U, [subviews count]);
  // There should be four "other profiles" items, sorted alphabetically.
  // The "other profiles" start at index 0, and each have a separator. We
  // need to iterate through the profiles in the order displayed in the bubble,
  // which is opposite from the drawn order.
  int sortedNameIndex = 0;
  for (int i = 7; i >= 0; i -= 2) {
    // The item at index i is the separator.
    NSButton* button = static_cast<NSButton*>([subviews objectAtIndex:i-1]);
    EXPECT_TRUE(
        [[button title] isEqualToString:sortedNames[sortedNameIndex++]]);
  }
}

TEST_F(ProfileChooserControllerTest,
       LocalProfileActiveCardLinksWithNewManagement) {
  switches::EnableNewProfileManagementForTesting(
      CommandLine::ForCurrentProcess());
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There should be one "sign in" link.
  EXPECT_EQ(1U, [activeCardLinks count]);
  NSButton* signinLink =
      static_cast<NSButton*>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showInlineSigninPage:), [signinLink action]);
  EXPECT_EQ(controller(), [signinLink target]);
}

TEST_F(ProfileChooserControllerTest,
    LocalProfileActiveCardLinksWithNewMenu) {
  EnableNewAvatarMenuOnly();
  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There should be one "sign in" link.
  EXPECT_EQ(1U, [activeCardLinks count]);
  NSButton* signinLink =
      static_cast<NSButton*>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showTabbedSigninPage:), [signinLink action]);
  EXPECT_EQ(controller(), [signinLink target]);
}

TEST_F(ProfileChooserControllerTest,
       SignedInProfileActiveCardLinksWithNewManagement) {
  switches::EnableNewProfileManagementForTesting(
      CommandLine::ForCurrentProcess());
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:2] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There is one link: manage accounts.
  EXPECT_EQ(1U, [activeCardLinks count]);
  NSButton* manageAccountsLink =
      static_cast<NSButton*>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountManagement:), [manageAccountsLink action]);
  EXPECT_EQ(controller(), [manageAccountsLink target]);
}

TEST_F(ProfileChooserControllerTest,
    SignedInProfileActiveCardLinksWithNewMenu) {
  EnableNewAvatarMenuOnly();
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  StartProfileChooserController();
  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  NSArray* activeCardLinks = [[activeCardSubviews objectAtIndex:0] subviews];

  // There is one link, without a target and with the user's email.
  EXPECT_EQ(1U, [activeCardLinks count]);
  NSButton* emailLink =
      static_cast<NSButton*>([activeCardLinks objectAtIndex:0]);
  EXPECT_EQ(nil, [emailLink action]);
  EXPECT_EQ(kEmail, base::SysNSStringToUTF8([emailLink title]));
  EXPECT_EQ(controller(), [emailLink target]);
}

TEST_F(ProfileChooserControllerTest, AccountManagementLayout) {
  switches::EnableNewProfileManagementForTesting(
      CommandLine::ForCurrentProcess());
  // Sign in the first profile.
  ProfileInfoCache* cache = testing_profile_manager()->profile_info_cache();
  cache->SetUserNameOfProfileAtIndex(0, base::ASCIIToUTF16(kEmail));

  // Set up the signin manager and the OAuth2Tokens.
  Profile* profile = browser()->profile();
  SigninManagerFactory::GetForProfile(profile)->
      SetAuthenticatedUsername(kEmail);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      UpdateCredentials(kEmail, kLoginToken);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      UpdateCredentials(kSecondaryEmail, kLoginToken);

  StartProfileChooserController();
  [controller() initMenuContentsWithView:
      profiles::BUBBLE_VIEW_MODE_ACCOUNT_MANAGEMENT];

  NSArray* subviews = [[[controller() window] contentView] subviews];
  EXPECT_EQ(1U, [subviews count]);
  subviews = [[subviews objectAtIndex:0] subviews];

  // There should be one active card, one accounts container, two separators
  // and one option buttons view.
  EXPECT_EQ(5U, [subviews count]);

  // There should be two buttons and a separator in the option buttons view.
  NSArray* buttonSubviews = [[subviews objectAtIndex:0] subviews];
  EXPECT_EQ(3U, [buttonSubviews count]);

  NSButton* notYouButton =
      static_cast<NSButton*>([buttonSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(showUserManager:), [notYouButton action]);
  EXPECT_EQ(controller(), [notYouButton target]);

  EXPECT_TRUE([[buttonSubviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  NSButton* lockButton =
      static_cast<NSButton*>([buttonSubviews objectAtIndex:2]);
  EXPECT_EQ(@selector(lockProfile:), [lockButton action]);
  EXPECT_EQ(controller(), [lockButton target]);

  // There should be a separator.
  EXPECT_TRUE([[subviews objectAtIndex:1] isKindOfClass:[NSBox class]]);

  // In the accounts view, there should be the account list container
  // accounts and one "add accounts" button.
  NSArray* accountsSubviews = [[subviews objectAtIndex:2] subviews];
  EXPECT_EQ(2U, [accountsSubviews count]);

  NSButton* addAccountsButton =
      static_cast<NSButton*>([accountsSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(addAccount:), [addAccountsButton action]);
  EXPECT_EQ(controller(), [addAccountsButton target]);

  // There should be two accounts in the account list container.
  NSArray* accountsListSubviews = [[accountsSubviews objectAtIndex:1] subviews];
  EXPECT_EQ(2U, [accountsListSubviews count]);

  NSButton* genericAccount =
      static_cast<NSButton*>([accountsListSubviews objectAtIndex:0]);
  NSButton* genericAccountDelete =
      static_cast<NSButton*>([[genericAccount subviews] objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountRemovalView:), [genericAccountDelete action]);
  EXPECT_EQ(controller(), [genericAccountDelete target]);
  EXPECT_NE(-1, [genericAccountDelete tag]);

  // Primary accounts are always last.
  NSButton* primaryAccount =
      static_cast<NSButton*>([accountsListSubviews objectAtIndex:1]);
  NSButton* primaryAccountDelete =
      static_cast<NSButton*>([[primaryAccount subviews] objectAtIndex:0]);
  EXPECT_EQ(@selector(showAccountRemovalView:), [primaryAccountDelete action]);
  EXPECT_EQ(controller(), [primaryAccountDelete target]);
  EXPECT_EQ(-1, [primaryAccountDelete tag]);

  // There should be another separator.
  EXPECT_TRUE([[subviews objectAtIndex:3] isKindOfClass:[NSBox class]]);

  // There should be the profile avatar, name and a "hide accounts" link
  // container in the active card view.
  NSArray* activeCardSubviews = [[subviews objectAtIndex:4] subviews];
  EXPECT_EQ(3U, [activeCardSubviews count]);

  // Profile icon.
  NSView* activeProfileImage = [activeCardSubviews objectAtIndex:2];
  EXPECT_TRUE([activeProfileImage isKindOfClass:[NSImageView class]]);

  // Profile name.
  NSView* activeProfileName = [activeCardSubviews objectAtIndex:1];
  EXPECT_TRUE([activeProfileName isKindOfClass:[NSButton class]]);
  EXPECT_EQ(menu()->GetItemAt(0).name, base::SysNSStringToUTF16(
      [static_cast<NSButton*>(activeProfileName) title]));

  // Profile links. This is a local profile, so there should be a signin button.
  NSArray* linksSubviews = [[activeCardSubviews objectAtIndex:0] subviews];
  EXPECT_EQ(1U, [linksSubviews count]);
  NSButton* link = static_cast<NSButton*>([linksSubviews objectAtIndex:0]);
  EXPECT_EQ(@selector(hideAccountManagement:), [link action]);
  EXPECT_EQ(controller(), [link target]);
}
