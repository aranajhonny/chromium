// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "webkit/common/blob/scoped_file.h"

namespace google_apis {
class ChangeResource;
class FileResource;
class ResourceEntry;
}

namespace leveldb {
class DB;
class WriteBatch;
}

namespace sync_file_system {
namespace drive_backend {

void PutVersionToBatch(int64 version, leveldb::WriteBatch* batch);

void PutServiceMetadataToBatch(const ServiceMetadata& service_metadata,
                               leveldb::WriteBatch* batch);
void PutFileMetadataToBatch(const FileMetadata& file,
                            leveldb::WriteBatch* batch);
void PutFileTrackerToBatch(const FileTracker& tracker,
                           leveldb::WriteBatch* batch);

void PutFileMetadataDeletionToBatch(const std::string& file_id,
                                    leveldb::WriteBatch* batch);
void PutFileTrackerDeletionToBatch(int64 tracker_id,
                                   leveldb::WriteBatch* batch);

bool HasFileAsParent(const FileDetails& details, const std::string& file_id);

bool IsAppRoot(const FileTracker& tracker);

std::string GetTrackerTitle(const FileTracker& tracker);

SyncStatusCode GDataErrorCodeToSyncStatusCode(
    google_apis::GDataErrorCode error);

// Returns true if |str| starts with |prefix|, and removes |prefix| from |str|.
// If |out| is not NULL, the result is stored in it.
bool RemovePrefix(const std::string& str, const std::string& prefix,
                  std::string* out);

scoped_ptr<ServiceMetadata> InitializeServiceMetadata(leveldb::DB* db);

template <typename Src, typename Dest>
void AppendContents(const Src& src, Dest* dest) {
  dest->insert(dest->end(), src.begin(), src.end());
}

template <typename Container>
const typename Container::mapped_type& LookUpMap(
    const Container& container,
    const typename Container::key_type& key,
    const typename Container::mapped_type& default_value) {
  typename Container::const_iterator found = container.find(key);
  if (found == container.end())
    return default_value;
  return found->second;
}

template <typename R, typename S, typename T>
R ComposeFunction(const base::Callback<T()>& g,
                  const base::Callback<R(S)>& f) {
  return f.Run(g.Run());
}

template <typename R, typename S, typename T>
base::Callback<R()> CreateComposedFunction(
    const base::Callback<T()>& g,
    const base::Callback<R(S)>& f) {
  return base::Bind(&ComposeFunction<R, S, T>, g, f);
}

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_BACKEND_UTIL_H_
