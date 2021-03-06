// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/fake_sync_worker.h"

#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"

namespace sync_file_system {
namespace drive_backend {

FakeSyncWorker::FakeSyncWorker()
    : sync_enabled_(true),
      has_refresh_token_(true),
      network_available_(true) {
  sequence_checker_.DetachFromSequence();
}

FakeSyncWorker::~FakeSyncWorker() {
  observers_.Clear();
}

void FakeSyncWorker::Initialize(
    scoped_ptr<SyncEngineContext> sync_engine_context) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  sync_engine_context_ = sync_engine_context.Pass();
  status_map_.clear();
  // TODO(peria): Set |status_map_| as a fake metadata database.
}

void FakeSyncWorker::RegisterOrigin(const GURL& origin,
                                    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  // TODO(peria): Check how it should act on installing installed app?
  status_map_[origin] = REGISTERED;
  callback.Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::EnableOrigin(const GURL& origin,
                                  const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  // TODO(peria): Check how it should act on enabling non-installed app?
  status_map_[origin] = ENABLED;
  callback.Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::DisableOrigin(const GURL& origin,
                                   const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  // TODO(peria): Check how it should act on disabling non-installed app?
  status_map_[origin] = DISABLED;
  callback.Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::UninstallOrigin(const GURL& origin,
                                     RemoteFileSyncService::UninstallFlag flag,
                                     const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  // TODO(peria): Check how it should act on uninstalling non-installed app?
  status_map_[origin] = UNINSTALLED;
  callback.Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::ProcessRemoteChange(
    const SyncFileCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  callback.Run(SYNC_STATUS_OK, fileapi::FileSystemURL());
}

void FakeSyncWorker::SetRemoteChangeProcessor(
    RemoteChangeProcessorOnWorker* remote_change_processor_on_worker) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

RemoteServiceState FakeSyncWorker::GetCurrentState() const {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return REMOTE_SERVICE_OK;
}

void FakeSyncWorker::GetOriginStatusMap(
    const RemoteFileSyncService::StatusMapCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  scoped_ptr<RemoteFileSyncService::OriginStatusMap>
      status_map(new RemoteFileSyncService::OriginStatusMap);
  for (StatusMap::const_iterator itr = status_map_.begin();
       itr != status_map_.end(); ++itr) {
    switch (itr->second) {
    case REGISTERED:
      (*status_map)[itr->first] = "Registered";
      break;
    case ENABLED:
      (*status_map)[itr->first] = "Enabled";
      break;
    case DISABLED:
      (*status_map)[itr->first] = "Disabled";
      break;
    case UNINSTALLED:
      (*status_map)[itr->first] = "Uninstalled";
      break;
    default:
      (*status_map)[itr->first] = "Unknown";
      break;
    }
  }
  callback.Run(status_map.Pass());
}

scoped_ptr<base::ListValue> FakeSyncWorker::DumpFiles(const GURL& origin) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return scoped_ptr<base::ListValue>();
}

scoped_ptr<base::ListValue> FakeSyncWorker::DumpDatabase() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  return scoped_ptr<base::ListValue>();
}

void FakeSyncWorker::SetSyncEnabled(bool enabled) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  sync_enabled_ = enabled;

  if (enabled)
    UpdateServiceState(REMOTE_SERVICE_OK, "Set FakeSyncWorker enabled.");
  else
    UpdateServiceState(REMOTE_SERVICE_DISABLED, "Disabled FakeSyncWorker.");
}

void FakeSyncWorker::PromoteDemotedChanges(const base::Closure& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  FOR_EACH_OBSERVER(
      Observer,
      observers_,
      OnPendingFileListUpdated(10));
  callback.Run();
}

void FakeSyncWorker::ApplyLocalChange(
    const FileChange& local_change,
    const base::FilePath& local_path,
    const SyncFileMetadata& local_metadata,
    const fileapi::FileSystemURL& url,
    const SyncStatusCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  callback.Run(SYNC_STATUS_OK);
}

void FakeSyncWorker::OnNotificationReceived() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  UpdateServiceState(REMOTE_SERVICE_OK, "Got push notification for Drive.");
}

void FakeSyncWorker::OnReadyToSendRequests() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  has_refresh_token_ = true;
  UpdateServiceState(REMOTE_SERVICE_OK, "ReadyToSendRequests");
}

void FakeSyncWorker::OnRefreshTokenInvalid() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  has_refresh_token_ = false;
  UpdateServiceState(REMOTE_SERVICE_OK, "RefreshTokenInvalid");
}

void FakeSyncWorker::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  bool new_network_availability =
      type != net::NetworkChangeNotifier::CONNECTION_NONE;
  if (network_available_ && !new_network_availability) {
    UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE, "Disconnected");
  } else if (!network_available_ && new_network_availability) {
    UpdateServiceState(REMOTE_SERVICE_OK, "Connected");
  }
  network_available_ = new_network_availability;
}

void FakeSyncWorker::DetachFromSequence() {
  sequence_checker_.DetachFromSequence();
}

void FakeSyncWorker::AddObserver(Observer* observer) {
  // This method is called on UI thread.
  observers_.AddObserver(observer);
}

void FakeSyncWorker::SetHasRefreshToken(bool has_refresh_token) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  has_refresh_token_ = has_refresh_token;
}

void FakeSyncWorker::UpdateServiceState(RemoteServiceState state,
                                        const std::string& description) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  FOR_EACH_OBSERVER(
      Observer, observers_,
      UpdateServiceState(state, description));
}

}  // namespace drive_backend
}  // namespace sync_file_system
