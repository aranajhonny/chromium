// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace test {

class AppSearchProviderTest : public AppListTestBase {
 public:
  AppSearchProviderTest() {}
  virtual ~AppSearchProviderTest() {}

  // AppListTestBase overrides:
  virtual void SetUp() OVERRIDE {
    AppListTestBase::SetUp();

    app_search_.reset(new AppSearchProvider(profile_.get(), NULL));
  }

  std::string RunQuery(const std::string& query) {
    app_search_->Start(base::UTF8ToUTF16(query));
    app_search_->Stop();

    std::string result_str;
    const SearchProvider::Results& results = app_search_->results();
    for (size_t i = 0; i < results.size(); ++i) {
      if (!result_str.empty())
        result_str += ',';

      result_str += base::UTF16ToUTF8(results[i]->title());
    }
    return result_str;
  }

 private:
  scoped_ptr<AppSearchProvider> app_search_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProviderTest);
};

TEST_F(AppSearchProviderTest, Basic) {
  EXPECT_EQ("", RunQuery(""));
  EXPECT_EQ("", RunQuery("!@#$-,-_"));
  EXPECT_EQ("", RunQuery("unmatched query"));

  // Search for "pa" should return both packaged app. The order is undefined
  // because the test only considers textual relevance and the two apps end
  // up having the same score.
  const std::string result = RunQuery("pa");
  EXPECT_TRUE(result == "Packaged App 1,Packaged App 2" ||
              result == "Packaged App 2,Packaged App 1");

  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  EXPECT_EQ("Packaged App 2", RunQuery("pa2"));
  EXPECT_EQ("Packaged App 1", RunQuery("app1"));
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, DisableAndEnable) {
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->DisableExtension(kHostedAppId,
                             extensions::Extension::DISABLE_NONE);
  EXPECT_EQ("Hosted App", RunQuery("host"));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ("Hosted App", RunQuery("host"));
}

TEST_F(AppSearchProviderTest, Uninstall) {
  EXPECT_EQ("Packaged App 1", RunQuery("pa1"));
  service_->UninstallExtension(
      kPackagedApp1Id, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
  EXPECT_EQ("", RunQuery("pa1"));

  // Let uninstall code to clean up.
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace app_list
