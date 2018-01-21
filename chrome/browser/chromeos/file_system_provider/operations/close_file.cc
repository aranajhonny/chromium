// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/close_file.h"

#include <string>

#include "chrome/common/extensions/api/file_system_provider.h"
#include "chrome/common/extensions/api/file_system_provider_internal.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {

CloseFile::CloseFile(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info,
                     int open_request_id,
                     const fileapi::AsyncFileUtil::StatusCallback& callback)
    : Operation(event_router, file_system_info),
      open_request_id_(open_request_id),
      callback_(callback) {
}

CloseFile::~CloseFile() {
}

bool CloseFile::Execute(int request_id) {
  scoped_ptr<base::DictionaryValue> values(new base::DictionaryValue);
  values->SetInteger("openRequestId", open_request_id_);

  return SendEvent(
      request_id,
      extensions::api::file_system_provider::OnCloseFileRequested::kEventName,
      values.Pass());
}

void CloseFile::OnSuccess(int /* request_id */,
                          scoped_ptr<RequestValue> result,
                          bool has_more) {
  callback_.Run(base::File::FILE_OK);
}

void CloseFile::OnError(int /* request_id */,
                        scoped_ptr<RequestValue> /* result */,
                        base::File::Error error) {
  callback_.Run(error);
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
