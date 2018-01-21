// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_rollback_manager_base.h"

#include "base/bind.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_internal_components_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

void OnConfigDone(bool success) {
  EXPECT_TRUE(success);
}

class SyncRollbackManagerBaseTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    TestInternalComponentsFactory factory(InternalComponentsFactory::Switches(),
                                          STORAGE_IN_MEMORY);
    manager_.Init(base::FilePath(base::FilePath::kCurrentDirectory),
                  MakeWeakHandle(base::WeakPtr<JsEventHandler>()),
                  "", 0, true, scoped_ptr<HttpPostProviderFactory>().Pass(),
                  std::vector<scoped_refptr<ModelSafeWorker> >(),
                  NULL, NULL, SyncCredentials(), "", "", "", &factory,
                  NULL, scoped_ptr<UnrecoverableErrorHandler>().Pass(),
                  NULL, NULL);
  }

  SyncRollbackManagerBase manager_;
  base::MessageLoop loop_;    // Needed for WeakHandle
};

TEST_F(SyncRollbackManagerBaseTest, InitTypeOnConfiguration) {
  EXPECT_TRUE(manager_.InitialSyncEndedTypes().Empty());

  manager_.ConfigureSyncer(
      CONFIGURE_REASON_NEW_CLIENT,
      ModelTypeSet(PREFERENCES, BOOKMARKS),
      ModelTypeSet(), ModelTypeSet(), ModelTypeSet(), ModelSafeRoutingInfo(),
      base::Bind(&OnConfigDone, true),
      base::Bind(&OnConfigDone, false));

  ReadTransaction trans(FROM_HERE, manager_.GetUserShare());
  ReadNode pref_root(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            pref_root.InitTypeRoot(PREFERENCES));

  ReadNode bookmark_root(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            bookmark_root.InitTypeRoot(BOOKMARKS));
  ReadNode bookmark_bar(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            bookmark_bar.InitByTagLookupForBookmarks("bookmark_bar"));
  ReadNode bookmark_mobile(&trans);
  EXPECT_EQ(BaseNode::INIT_FAILED_ENTRY_NOT_GOOD,
            bookmark_mobile.InitByTagLookupForBookmarks("synced_bookmarks"));
  ReadNode bookmark_other(&trans);
  EXPECT_EQ(BaseNode::INIT_OK,
            bookmark_other.InitByTagLookupForBookmarks("other_bookmarks"));
}

}  // anonymous namespace

}  // namespace syncer
