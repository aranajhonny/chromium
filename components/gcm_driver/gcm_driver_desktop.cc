// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver_desktop.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/system_encryptor.h"
#include "net/base/ip_endpoint.h"
#include "net/url_request/url_request_context_getter.h"

namespace gcm {

namespace {

// Empty string is reserved for the default app handler.
const char kDefaultAppHandler[] = "";

}  // namespace

// Helper class to save tasks to run until we're ready to execute them.
class GCMDriverDesktop::DelayedTaskController {
 public:
  DelayedTaskController();
  ~DelayedTaskController();

  // Adds a task that will be invoked once we're ready.
  void AddTask(const base::Closure& task);

  // Sets ready status. It is ready only when check-in is completed and
  // the GCMClient is fully initialized.
  void SetReady();

  // Returns true if it is ready to perform tasks.
  bool CanRunTaskWithoutDelay() const;

 private:
  void RunTasks();

  // Flag that indicates that GCM is ready.
  bool ready_;

  std::vector<base::Closure> delayed_tasks_;

  DISALLOW_COPY_AND_ASSIGN(DelayedTaskController);
};

GCMDriverDesktop::DelayedTaskController::DelayedTaskController()
    : ready_(false) {
}

GCMDriverDesktop::DelayedTaskController::~DelayedTaskController() {
}

void GCMDriverDesktop::DelayedTaskController::AddTask(
    const base::Closure& task) {
  delayed_tasks_.push_back(task);
}

void GCMDriverDesktop::DelayedTaskController::SetReady() {
  ready_ = true;
  RunTasks();
}

bool GCMDriverDesktop::DelayedTaskController::CanRunTaskWithoutDelay() const {
  return ready_;
}

void GCMDriverDesktop::DelayedTaskController::RunTasks() {
  DCHECK(ready_);

  for (size_t i = 0; i < delayed_tasks_.size(); ++i)
    delayed_tasks_[i].Run();
  delayed_tasks_.clear();
}

class GCMDriverDesktop::IOWorker : public GCMClient::Delegate {
 public:
  // Called on UI thread.
  IOWorker(const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
           const scoped_refptr<base::SequencedTaskRunner>& io_thread);
  virtual ~IOWorker();

  // Overridden from GCMClient::Delegate:
  // Called on IO thread.
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnUnregisterFinished(const std::string& app_id,
                                    GCMClient::Result result) OVERRIDE;
  virtual void OnSendFinished(const std::string& app_id,
                              const std::string& message_id,
                              GCMClient::Result result) OVERRIDE;
  virtual void OnMessageReceived(
      const std::string& app_id,
      const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnMessageSendError(
      const std::string& app_id,
      const GCMClient::SendErrorDetails& send_error_details) OVERRIDE;
  virtual void OnGCMReady() OVERRIDE;
  virtual void OnActivityRecorded() OVERRIDE;
  virtual void OnConnected(const net::IPEndPoint& ip_endpoint) OVERRIDE;
  virtual void OnDisconnected() OVERRIDE;

  // Called on IO thread.
  void Initialize(
      scoped_ptr<GCMClientFactory> gcm_client_factory,
      const GCMClient::ChromeBuildInfo& chrome_build_info,
      const base::FilePath& store_path,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  void Start(const base::WeakPtr<GCMDriverDesktop>& service);
  void Stop();
  void CheckOut();
  void Register(const std::string& app_id,
                const std::vector<std::string>& sender_ids);
  void Unregister(const std::string& app_id);
  void Send(const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message);
  void GetGCMStatistics(bool clear_logs);
  void SetGCMRecording(bool recording);

  void SetAccountsForCheckin(
      const std::map<std::string, std::string>& account_tokens);

  // For testing purpose. Can be called from UI thread. Use with care.
  GCMClient* gcm_client_for_testing() const { return gcm_client_.get(); }

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> io_thread_;

  base::WeakPtr<GCMDriverDesktop> service_;

  scoped_ptr<GCMClient> gcm_client_;

  DISALLOW_COPY_AND_ASSIGN(IOWorker);
};

GCMDriverDesktop::IOWorker::IOWorker(
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : ui_thread_(ui_thread),
      io_thread_(io_thread) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
}

GCMDriverDesktop::IOWorker::~IOWorker() {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());
}

void GCMDriverDesktop::IOWorker::Initialize(
    scoped_ptr<GCMClientFactory> gcm_client_factory,
    const GCMClient::ChromeBuildInfo& chrome_build_info,
    const base::FilePath& store_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  gcm_client_ = gcm_client_factory->BuildInstance();

  gcm_client_->Initialize(chrome_build_info,
                          store_path,
                          blocking_task_runner,
                          request_context,
                          make_scoped_ptr<Encryptor>(new SystemEncryptor),
                          this);
}

void GCMDriverDesktop::IOWorker::OnRegisterFinished(
    const std::string& app_id,
    const std::string& registration_id,
    GCMClient::Result result) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::RegisterFinished, service_, app_id,
                 registration_id, result));
}

void GCMDriverDesktop::IOWorker::OnUnregisterFinished(
    const std::string& app_id,
    GCMClient::Result result) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  ui_thread_->PostTask(FROM_HERE,
                       base::Bind(&GCMDriverDesktop::UnregisterFinished,
                                  service_,
                                  app_id,
                                  result));
}

void GCMDriverDesktop::IOWorker::OnSendFinished(const std::string& app_id,
                                                const std::string& message_id,
                                                GCMClient::Result result) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::SendFinished, service_, app_id, message_id,
                 result));
}

void GCMDriverDesktop::IOWorker::OnMessageReceived(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::MessageReceived,
                 service_,
                 app_id,
                 message));
}

void GCMDriverDesktop::IOWorker::OnMessagesDeleted(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::MessagesDeleted, service_, app_id));
}

void GCMDriverDesktop::IOWorker::OnMessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::MessageSendError, service_, app_id,
                 send_error_details));
}

void GCMDriverDesktop::IOWorker::OnGCMReady() {
  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::GCMClientReady, service_));
}

void GCMDriverDesktop::IOWorker::OnActivityRecorded() {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());
  // When an activity is recorded, get all the stats and refresh the UI of
  // gcm-internals page.
  GetGCMStatistics(false);
}

void GCMDriverDesktop::IOWorker::OnConnected(
    const net::IPEndPoint& ip_endpoint) {
  ui_thread_->PostTask(FROM_HERE,
                       base::Bind(&GCMDriverDesktop::OnConnected,
                                  service_,
                                  ip_endpoint));
}

void GCMDriverDesktop::IOWorker::OnDisconnected() {
  ui_thread_->PostTask(FROM_HERE,
                       base::Bind(&GCMDriverDesktop::OnDisconnected, service_));
}

void GCMDriverDesktop::IOWorker::Start(
    const base::WeakPtr<GCMDriverDesktop>& service) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  service_ = service;
  gcm_client_->Start();
}

void GCMDriverDesktop::IOWorker::Stop() {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  gcm_client_->Stop();
}

void GCMDriverDesktop::IOWorker::CheckOut() {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  gcm_client_->CheckOut();

  // Note that we still need to keep GCMClient instance alive since the
  // GCMDriverDesktop may check in again.
}

void GCMDriverDesktop::IOWorker::Register(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  gcm_client_->Register(app_id, sender_ids);
}

void GCMDriverDesktop::IOWorker::Unregister(const std::string& app_id) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  gcm_client_->Unregister(app_id);
}

void GCMDriverDesktop::IOWorker::Send(
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  gcm_client_->Send(app_id, receiver_id, message);
}

void GCMDriverDesktop::IOWorker::GetGCMStatistics(bool clear_logs) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_.get()) {
    if (clear_logs)
      gcm_client_->ClearActivityLogs();
    stats = gcm_client_->GetStatistics();
  }

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::GetGCMStatisticsFinished, service_, stats));
}

void GCMDriverDesktop::IOWorker::SetGCMRecording(bool recording) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());
  gcm::GCMClient::GCMStatistics stats;

  if (gcm_client_.get()) {
    gcm_client_->SetRecording(recording);
    stats = gcm_client_->GetStatistics();
    stats.gcm_client_created = true;
  }

  ui_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::GetGCMStatisticsFinished, service_, stats));
}

void GCMDriverDesktop::IOWorker::SetAccountsForCheckin(
    const std::map<std::string, std::string>& account_tokens) {
  DCHECK(io_thread_->RunsTasksOnCurrentThread());

  if (gcm_client_.get())
    gcm_client_->SetAccountsForCheckin(account_tokens);
}

GCMDriverDesktop::GCMDriverDesktop(
    scoped_ptr<GCMClientFactory> gcm_client_factory,
    const GCMClient::ChromeBuildInfo& chrome_build_info,
    const base::FilePath& store_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : signed_in_(false),
      gcm_started_(false),
      gcm_enabled_(true),
      connected_(false),
      ui_thread_(ui_thread),
      io_thread_(io_thread),
      weak_ptr_factory_(this) {
  // Create and initialize the GCMClient. Note that this does not initiate the
  // GCM check-in.
  io_worker_.reset(new IOWorker(ui_thread, io_thread));
  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::Initialize,
                 base::Unretained(io_worker_.get()),
                 base::Passed(&gcm_client_factory),
                 chrome_build_info,
                 store_path,
                 request_context,
                 blocking_task_runner));
}

GCMDriverDesktop::~GCMDriverDesktop() {
}

void GCMDriverDesktop::Shutdown() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  GCMDriver::Shutdown();
  io_thread_->DeleteSoon(FROM_HERE, io_worker_.release());
}

void GCMDriverDesktop::OnSignedIn() {
  signed_in_ = true;
  EnsureStarted();
}

void GCMDriverDesktop::Purge() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // We still proceed with the check-out logic even if the check-in is not
  // initiated in the current session. This will make sure that all the
  // persisted data written previously will get purged.
  signed_in_ = false;
  RemoveCachedData();

  io_thread_->PostTask(FROM_HERE,
                       base::Bind(&GCMDriverDesktop::IOWorker::CheckOut,
                       base::Unretained(io_worker_.get())));
}

void GCMDriverDesktop::AddAppHandler(const std::string& app_id,
                                     GCMAppHandler* handler) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  GCMDriver::AddAppHandler(app_id, handler);

   // Ensures that the GCM service is started when there is an interest.
  EnsureStarted();
}

void GCMDriverDesktop::RemoveAppHandler(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  GCMDriver::RemoveAppHandler(app_id);

  // Stops the GCM service when no app intends to consume it.
  if (app_handlers().empty())
    Stop();
}

void GCMDriverDesktop::Enable() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  if (gcm_enabled_)
    return;
  gcm_enabled_ = true;

  EnsureStarted();
}

void GCMDriverDesktop::Disable() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  if (!gcm_enabled_)
    return;
  gcm_enabled_ = false;

  Stop();
}

void GCMDriverDesktop::Stop() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // No need to stop GCM service if not started yet.
  if (!gcm_started_)
    return;

  RemoveCachedData();

  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::Stop,
                 base::Unretained(io_worker_.get())));
}

void GCMDriverDesktop::RegisterImpl(
    const std::string& app_id,
    const std::vector<std::string>& sender_ids) {
  // Delay the register operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::Bind(&GCMDriverDesktop::DoRegister,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id,
                                                 sender_ids));
    return;
  }

  DoRegister(app_id, sender_ids);
}

void GCMDriverDesktop::DoRegister(const std::string& app_id,
                                  const std::vector<std::string>& sender_ids) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  if (!HasRegisterCallback(app_id)) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::Register,
                 base::Unretained(io_worker_.get()),
                 app_id,
                 sender_ids));
}

void GCMDriverDesktop::UnregisterImpl(const std::string& app_id) {
  // Delay the unregister operation until GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(
        base::Bind(&GCMDriverDesktop::DoUnregister,
                   weak_ptr_factory_.GetWeakPtr(),
                   app_id));
    return;
  }

  DoUnregister(app_id);
}

void GCMDriverDesktop::DoUnregister(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // Ask the server to unregister it. There could be a small chance that the
  // unregister request fails. If this occurs, it does not bring any harm since
  // we simply reject the messages/events received from the server.
  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::Unregister,
                 base::Unretained(io_worker_.get()),
                 app_id));
}

void GCMDriverDesktop::SendImpl(const std::string& app_id,
                                const std::string& receiver_id,
                                const GCMClient::OutgoingMessage& message) {
  // Delay the send operation until all GCMClient is ready.
  if (!delayed_task_controller_->CanRunTaskWithoutDelay()) {
    delayed_task_controller_->AddTask(base::Bind(&GCMDriverDesktop::DoSend,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 app_id,
                                                 receiver_id,
                                                 message));
    return;
  }

  DoSend(app_id, receiver_id, message);
}

void GCMDriverDesktop::DoSend(const std::string& app_id,
                              const std::string& receiver_id,
                              const GCMClient::OutgoingMessage& message) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::Send,
                 base::Unretained(io_worker_.get()),
                 app_id,
                 receiver_id,
                 message));
}

GCMClient* GCMDriverDesktop::GetGCMClientForTesting() const {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  return io_worker_ ? io_worker_->gcm_client_for_testing() : NULL;
}

bool GCMDriverDesktop::IsStarted() const {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  return gcm_started_;
}

bool GCMDriverDesktop::IsConnected() const {
  return connected_;
}

void GCMDriverDesktop::GetGCMStatistics(
    const GetGCMStatisticsCallback& callback,
    bool clear_logs) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  DCHECK(!callback.is_null());

  request_gcm_statistics_callback_ = callback;
  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::GetGCMStatistics,
                 base::Unretained(io_worker_.get()),
                 clear_logs));
}

void GCMDriverDesktop::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                       bool recording) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  request_gcm_statistics_callback_ = callback;
  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::SetGCMRecording,
                 base::Unretained(io_worker_.get()),
                 recording));
}

void GCMDriverDesktop::SetAccountsForCheckin(
    const std::map<std::string, std::string>& account_tokens) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::SetAccountsForCheckin,
                 base::Unretained(io_worker_.get()),
                 account_tokens));
}

GCMClient::Result GCMDriverDesktop::EnsureStarted() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  if (gcm_started_)
    return GCMClient::SUCCESS;

  if (!gcm_enabled_)
    return GCMClient::GCM_DISABLED;

  // Have any app requested the service?
  if (app_handlers().empty())
    return GCMClient::UNKNOWN_ERROR;

  // TODO(jianli): To be removed when sign-in enforcement is dropped.
  if (!signed_in_ && !GCMDriver::IsAllowedForAllUsers())
    return GCMClient::NOT_SIGNED_IN;

  DCHECK(!delayed_task_controller_);
  delayed_task_controller_.reset(new DelayedTaskController);

  // Note that we need to pass weak pointer again since the existing weak
  // pointer in IOWorker might have been invalidated when check-out occurs.
  io_thread_->PostTask(
      FROM_HERE,
      base::Bind(&GCMDriverDesktop::IOWorker::Start,
                 base::Unretained(io_worker_.get()),
                 weak_ptr_factory_.GetWeakPtr()));

  gcm_started_ = true;
  return GCMClient::SUCCESS;
}

void GCMDriverDesktop::RemoveCachedData() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());
  // Remove all the queued tasks since they no longer make sense after
  // GCM service is stopped.
  weak_ptr_factory_.InvalidateWeakPtrs();

  gcm_started_ = false;
  delayed_task_controller_.reset();
  ClearCallbacks();
}

void GCMDriverDesktop::MessageReceived(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  GetAppHandler(app_id)->OnMessage(app_id, message);
}

void GCMDriverDesktop::MessagesDeleted(const std::string& app_id) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  GetAppHandler(app_id)->OnMessagesDeleted(app_id);
}

void GCMDriverDesktop::MessageSendError(
    const std::string& app_id,
    const GCMClient::SendErrorDetails& send_error_details) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  GetAppHandler(app_id)->OnSendError(app_id, send_error_details);
}

void GCMDriverDesktop::GCMClientReady() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  delayed_task_controller_->SetReady();
}

void GCMDriverDesktop::OnConnected(const net::IPEndPoint& ip_endpoint) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  connected_ = true;

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  const GCMAppHandlerMap& app_handler_map = app_handlers();
  for (GCMAppHandlerMap::const_iterator iter = app_handler_map.begin();
       iter != app_handler_map.end(); ++iter) {
    iter->second->OnConnected(ip_endpoint);
  }

  GetAppHandler(kDefaultAppHandler)->OnConnected(ip_endpoint);
}

void GCMDriverDesktop::OnDisconnected() {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  connected_ = false;

  // Drop the event if the service has been stopped.
  if (!gcm_started_)
    return;

  const GCMAppHandlerMap& app_handler_map = app_handlers();
  for (GCMAppHandlerMap::const_iterator iter = app_handler_map.begin();
       iter != app_handler_map.end(); ++iter) {
    iter->second->OnDisconnected();
  }

  GetAppHandler(kDefaultAppHandler)->OnDisconnected();
}

void GCMDriverDesktop::GetGCMStatisticsFinished(
    const GCMClient::GCMStatistics& stats) {
  DCHECK(ui_thread_->RunsTasksOnCurrentThread());

  // Normally request_gcm_statistics_callback_ would not be null.
  if (!request_gcm_statistics_callback_.is_null())
    request_gcm_statistics_callback_.Run(stats);
  else
    LOG(WARNING) << "request_gcm_statistics_callback_ is NULL.";
}

}  // namespace gcm

