// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_client.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/pref_names.h"

namespace {

class MockHotwordService : public HotwordService {
 public:
  explicit MockHotwordService(Profile* profile)
      : HotwordService(profile), service_available_(true) {};
  virtual ~MockHotwordService() {}

  virtual bool IsServiceAvailable() OVERRIDE {
    return service_available_;
  }

  void setServiceAvailable(bool available) {
    service_available_ = available;
  }

  static KeyedService* Build(content::BrowserContext* profile) {
    return new MockHotwordService(static_cast<Profile*>(profile));
  }

 private:
  bool service_available_;

  DISALLOW_COPY_AND_ASSIGN(MockHotwordService);
};

class MockHotwordClient : public HotwordClient {
 public:
  MockHotwordClient()
      : last_enabled_(false),
        state_changed_count_(0),
        recognized_count_(0) {
  }

  virtual ~MockHotwordClient() {}

  virtual void OnHotwordStateChanged(bool enabled) OVERRIDE {
    last_enabled_ = enabled;
    state_changed_count_++;
  }

  virtual void OnHotwordRecognized() OVERRIDE {
    recognized_count_++;
  }

  bool last_enabled() const { return last_enabled_; }
  int state_changed_count() const { return state_changed_count_; }
  int recognized_count() const { return recognized_count_; }

 private:
  bool last_enabled_;
  int state_changed_count_;
  int recognized_count_;

  DISALLOW_COPY_AND_ASSIGN(MockHotwordClient);
};

class HotwordPrivateApiTest : public ExtensionApiTest {
 public:
  HotwordPrivateApiTest() {}
  virtual ~HotwordPrivateApiTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();

    test_data_dir_ = test_data_dir_.AppendASCII("hotword_private");

    service_ = static_cast<MockHotwordService*>(
        HotwordServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), MockHotwordService::Build));
  }

  MockHotwordService* service() {
    return service_;
  }

 private:
  MockHotwordService* service_;

  DISALLOW_COPY_AND_ASSIGN(HotwordPrivateApiTest);
};

}  // anonymous namespace

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, SetEnabled) {
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));

  ExtensionTestMessageListener listenerTrue("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setEnabledTrue")) << message_;
  EXPECT_TRUE(listenerTrue.WaitUntilSatisfied());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));

  ExtensionTestMessageListener listenerFalse("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setEnabledFalse")) << message_;
  EXPECT_TRUE(listenerFalse.WaitUntilSatisfied());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHotwordSearchEnabled));
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, SetAudioLoggingEnabled) {
  EXPECT_FALSE(service()->IsOptedIntoAudioLogging());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled));

  ExtensionTestMessageListener listenerTrue("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setAudioLoggingEnableTrue"))
      << message_;
  EXPECT_TRUE(listenerTrue.WaitUntilSatisfied());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled));
  EXPECT_TRUE(service()->IsOptedIntoAudioLogging());

  ExtensionTestMessageListener listenerFalse("ready", false);
  ASSERT_TRUE(RunComponentExtensionTest("setAudioLoggingEnableFalse"))
      << message_;
  EXPECT_TRUE(listenerFalse.WaitUntilSatisfied());
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kHotwordAudioLoggingEnabled));
  EXPECT_FALSE(service()->IsOptedIntoAudioLogging());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, GetStatus) {
  ASSERT_TRUE(RunComponentExtensionTest("getEnabled")) << message_;
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, IsAvailableTrue) {
  service()->setServiceAvailable(true);
  ExtensionTestMessageListener listener("available: true", false);
  ASSERT_TRUE(RunComponentExtensionTest("isAvailable")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, IsAvailableFalse) {
  service()->setServiceAvailable(false);
  ExtensionTestMessageListener listener("available: false", false);
  ASSERT_TRUE(RunComponentExtensionTest("isAvailable")) << message_;
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, OnEnabledChanged) {
  // Trigger the pref registrar.
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtensionAsComponent(test_data_dir_.AppendASCII("onEnabledChanged")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerNotification("notification", false);
  profile()->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, true);
  EXPECT_TRUE(listenerNotification.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(HotwordPrivateApiTest, HotwordSession) {
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  ExtensionTestMessageListener listener("ready", false);
  LoadExtensionAsComponent(
      test_data_dir_.AppendASCII("hotwordSession"));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  ExtensionTestMessageListener listenerStopReady("stopReady", false);
  ExtensionTestMessageListener listenerStopped("stopped", false);
  MockHotwordClient client;
  service()->RequestHotwordSession(&client);
  EXPECT_TRUE(listenerStopReady.WaitUntilSatisfied());
  service()->StopHotwordSession(&client);
  EXPECT_TRUE(listenerStopped.WaitUntilSatisfied());

  EXPECT_TRUE(client.last_enabled());
  EXPECT_EQ(1, client.state_changed_count());
  EXPECT_EQ(1, client.recognized_count());
}
