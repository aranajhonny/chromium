// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_task_executor.h"

#include <set>
#include <string>

#include "base/run_loop.h"
#include "chrome/browser/chromeos/drive/fake_file_system.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace drive {

namespace {

// Test harness for verifying the behavior of FileTaskExecutor.
class TestDelegate : public FileTaskExecutorDelegate {
 public:
  explicit TestDelegate(std::set<std::string>* opend_urls)
      : opend_urls_(opend_urls),
        fake_drive_service_(new FakeDriveService),
        fake_file_system_(new test_util::FakeFileSystem(
            fake_drive_service_.get())) {
    fake_drive_service_->set_open_url_format("http://openlink/%s/%s");
  }

  // FileTaskExecutorDelegate overrides.
  virtual FileSystemInterface* GetFileSystem() OVERRIDE {
    return fake_file_system_.get();
  }

  virtual DriveServiceInterface* GetDriveService() OVERRIDE {
    return fake_drive_service_.get();
  }

  virtual void OpenBrowserWindow(const GURL& open_link) OVERRIDE {
    opend_urls_->insert(open_link.spec());
  }

  // Sets up files on the fake Drive service.
  bool SetUpTestFiles() {
    {
      google_apis::GDataErrorCode result = google_apis::GDATA_OTHER_ERROR;
      scoped_ptr<google_apis::FileResource> file;
      fake_drive_service_->AddNewFileWithResourceId(
          "id1",
          "text/plain",
          "random data",
          fake_drive_service_->GetRootResourceId(),
          "file1.txt",
          false,
          google_apis::test_util::CreateCopyResultCallback(&result, &file));
      base::RunLoop().RunUntilIdle();
      if (result != google_apis::HTTP_CREATED)
        return false;
    }
    {
      google_apis::GDataErrorCode result = google_apis::GDATA_OTHER_ERROR;
      scoped_ptr<google_apis::FileResource> file;
      fake_drive_service_->AddNewFileWithResourceId(
          "id2",
          "text/plain",
          "random data",
          fake_drive_service_->GetRootResourceId(),
          "file2.txt",
          false,
          google_apis::test_util::CreateCopyResultCallback(&result, &file));
      base::RunLoop().RunUntilIdle();
      if (result != google_apis::HTTP_CREATED)
        return false;
    }
    return true;
  }

 private:
  std::set<std::string>* const opend_urls_;
  scoped_ptr<FakeDriveService> fake_drive_service_;
  scoped_ptr<test_util::FakeFileSystem> fake_file_system_;
};

}  // namespace

TEST(FileTaskExecutorTest, DriveAppOpenSuccess) {
  content::TestBrowserThreadBundle thread_bundle;

  std::set<std::string> opend_urls;

  // |delegate_ptr| will be owned by |executor|.
  TestDelegate* const delegate_ptr = new TestDelegate(&opend_urls);
  ASSERT_TRUE(delegate_ptr->SetUpTestFiles());
  // |executor| deletes itself after Execute() is finished.
  FileTaskExecutor* const executor = new FileTaskExecutor(
      scoped_ptr<FileTaskExecutorDelegate>(delegate_ptr), "test-app-id");

  std::vector<fileapi::FileSystemURL> urls;
  urls.push_back(fileapi::FileSystemURL::CreateForTest(
      GURL("http://origin/"),
      fileapi::kFileSystemTypeDrive,
      base::FilePath::FromUTF8Unsafe("/special/drive/root/file1.txt")));
  urls.push_back(fileapi::FileSystemURL::CreateForTest(
      GURL("http://origin/"),
      fileapi::kFileSystemTypeDrive,
      base::FilePath::FromUTF8Unsafe("/special/drive/root/file2.txt")));

  extensions::api::file_browser_private::TaskResult result =
      extensions::api::file_browser_private::TASK_RESULT_NONE;
  executor->Execute(urls,
                    google_apis::test_util::CreateCopyResultCallback(&result));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(extensions::api::file_browser_private::TASK_RESULT_OPENED, result);
  ASSERT_EQ(2u, opend_urls.size());
  EXPECT_TRUE(opend_urls.count("http://openlink/id1/test-app-id"));
  EXPECT_TRUE(opend_urls.count("http://openlink/id2/test-app-id"));
}

TEST(FileTaskExecutorTest, DriveAppOpenFailForNonExistingFile) {
  content::TestBrowserThreadBundle thread_bundle;

  std::set<std::string> opend_urls;

  // |delegate_ptr| will be owned by |executor|.
  TestDelegate* const delegate_ptr = new TestDelegate(&opend_urls);
  ASSERT_TRUE(delegate_ptr->SetUpTestFiles());
  // |executor| deletes itself after Execute() is finished.
  FileTaskExecutor* const executor = new FileTaskExecutor(
      scoped_ptr<FileTaskExecutorDelegate>(delegate_ptr), "test-app-id");

  std::vector<fileapi::FileSystemURL> urls;
  urls.push_back(fileapi::FileSystemURL::CreateForTest(
      GURL("http://origin/"),
      fileapi::kFileSystemTypeDrive,
      base::FilePath::FromUTF8Unsafe("/special/drive/root/not-exist.txt")));

  extensions::api::file_browser_private::TaskResult result =
      extensions::api::file_browser_private::TASK_RESULT_NONE;
  executor->Execute(urls,
                    google_apis::test_util::CreateCopyResultCallback(&result));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(extensions::api::file_browser_private::TASK_RESULT_FAILED, result);
  ASSERT_TRUE(opend_urls.empty());
}

}  // namespace drive
