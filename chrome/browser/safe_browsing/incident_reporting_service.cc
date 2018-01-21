// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting_service.h"

#include <math.h>

#include <algorithm>
#include <vector>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/process/process_info.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/tracked/tracked_preference_validation_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/environment_data_collection.h"
#include "chrome/browser/safe_browsing/incident_report_uploader_impl.h"
#include "chrome/browser/safe_browsing/preference_validation_delegate.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/url_request_context_getter.h"

namespace safe_browsing {

namespace {

enum IncidentType {
  // Start with 1 rather than zero; otherwise there won't be enough buckets for
  // the histogram.
  TRACKED_PREFERENCE = 1,
  NUM_INCIDENT_TYPES
};

enum IncidentDisposition {
  DROPPED,
  ACCEPTED,
};

const int64 kDefaultUploadDelayMs = 1000 * 60;  // one minute

void LogIncidentDataType(
    IncidentDisposition disposition,
    const ClientIncidentReport_IncidentData& incident_data) {
  IncidentType type = TRACKED_PREFERENCE;

  // Add a switch statement once other types are supported.
  DCHECK(incident_data.has_tracked_preference());

  if (disposition == ACCEPTED) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.Incident", type, NUM_INCIDENT_TYPES);
  } else {
    DCHECK_EQ(disposition, DROPPED);
    UMA_HISTOGRAM_ENUMERATION("SBIRS.DroppedIncident", type,
                              NUM_INCIDENT_TYPES);
  }
}

}  // namespace

struct IncidentReportingService::ProfileContext {
  ProfileContext();
  ~ProfileContext();

  // The incidents collected for this profile pending creation and/or upload.
  ScopedVector<ClientIncidentReport_IncidentData> incidents;

  // False until PROFILE_ADDED notification is received.
  bool added;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileContext);
};

class IncidentReportingService::UploadContext {
 public:
  explicit UploadContext(scoped_ptr<ClientIncidentReport> report);
  ~UploadContext();

  // The report being uploaded.
  scoped_ptr<ClientIncidentReport> report;

  // The uploader in use. This is NULL until the CSD killswitch is checked.
  scoped_ptr<IncidentReportUploader> uploader;

  // The set of profiles from which incidents in |report| originated.
  std::vector<Profile*> profiles;

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadContext);
};

IncidentReportingService::ProfileContext::ProfileContext() : added() {
}

IncidentReportingService::ProfileContext::~ProfileContext() {
}

IncidentReportingService::UploadContext::UploadContext(
    scoped_ptr<ClientIncidentReport> report)
    : report(report.Pass()) {
}

IncidentReportingService::UploadContext::~UploadContext() {
}

IncidentReportingService::IncidentReportingService(
    SafeBrowsingService* safe_browsing_service,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter)
    : database_manager_(safe_browsing_service ?
                        safe_browsing_service->database_manager() : NULL),
      url_request_context_getter_(request_context_getter),
      collect_environment_data_fn_(&CollectEnvironmentData),
      environment_collection_task_runner_(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      environment_collection_pending_(),
      collection_timeout_pending_(),
      upload_timer_(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kDefaultUploadDelayMs),
                    this,
                    &IncidentReportingService::OnCollectionTimeout),
      receiver_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_ADDED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_PROFILE_DESTROYED,
                              content::NotificationService::AllSources());
}

IncidentReportingService::~IncidentReportingService() {
  CancelIncidentCollection();

  // Cancel all internal asynchronous tasks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  CancelEnvironmentCollection();
  CancelDownloadCollection();
  CancelAllReportUploads();

  STLDeleteValues(&profiles_);
}

AddIncidentCallback IncidentReportingService::GetAddIncidentCallback(
    Profile* profile) {
  // Force the context to be created so that incidents added before
  // OnProfileAdded is called are held until the profile's preferences can be
  // queried.
  ignore_result(GetOrCreateProfileContext(profile));

  return base::Bind(&IncidentReportingService::AddIncident,
                    receiver_weak_ptr_factory_.GetWeakPtr(),
                    profile);
}

scoped_ptr<TrackedPreferenceValidationDelegate>
IncidentReportingService::CreatePreferenceValidationDelegate(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (profile->IsOffTheRecord())
    return scoped_ptr<TrackedPreferenceValidationDelegate>();
  return scoped_ptr<TrackedPreferenceValidationDelegate>(
      new PreferenceValidationDelegate(GetAddIncidentCallback(profile)));
}

void IncidentReportingService::SetCollectEnvironmentHook(
    CollectEnvironmentDataFn collect_environment_data_hook,
    const scoped_refptr<base::TaskRunner>& task_runner) {
  if (collect_environment_data_hook) {
    collect_environment_data_fn_ = collect_environment_data_hook;
    environment_collection_task_runner_ = task_runner;
  } else {
    collect_environment_data_fn_ = &CollectEnvironmentData;
    environment_collection_task_runner_ =
        content::BrowserThread::GetBlockingPool()
            ->GetTaskRunnerWithShutdownBehavior(
                base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  }
}

void IncidentReportingService::OnProfileAdded(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Track the addition of all profiles even when no report is being assembled
  // so that the service can determine whether or not it can evaluate a
  // profile's preferences at the time of incident addition.
  ProfileContext* context = GetOrCreateProfileContext(profile);
  context->added = true;

  // Nothing else to do if a report is not being assembled.
  if (!report_)
    return;

  // Drop all incidents received prior to creation if the profile is not
  // participating in safe browsing.
  if (!context->incidents.empty() &&
      !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    for (size_t i = 0; i < context->incidents.size(); ++i) {
      LogIncidentDataType(DROPPED, *context->incidents[i]);
    }
    context->incidents.clear();
  }

  // Take another stab at finding the most recent download if a report is being
  // assembled and one hasn't been found yet (the LastDownloadFinder operates
  // only on profiles that have been added to the ProfileManager).
  BeginDownloadCollection();
}

scoped_ptr<LastDownloadFinder> IncidentReportingService::CreateDownloadFinder(
    const LastDownloadFinder::LastDownloadCallback& callback) {
  return LastDownloadFinder::Create(callback).Pass();
}

scoped_ptr<IncidentReportUploader> IncidentReportingService::StartReportUpload(
    const IncidentReportUploader::OnResultCallback& callback,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const ClientIncidentReport& report) {
  return IncidentReportUploaderImpl::UploadReport(
             callback, request_context_getter, report).Pass();
}

IncidentReportingService::ProfileContext*
IncidentReportingService::GetOrCreateProfileContext(Profile* profile) {
  ProfileContextCollection::iterator it =
      profiles_.insert(ProfileContextCollection::value_type(profile, NULL))
          .first;
  if (!it->second)
    it->second = new ProfileContext();
  return it->second;
}

IncidentReportingService::ProfileContext*
IncidentReportingService::GetProfileContext(Profile* profile) {
  ProfileContextCollection::iterator it = profiles_.find(profile);
  return it == profiles_.end() ? NULL : it->second;
}

void IncidentReportingService::OnProfileDestroyed(Profile* profile) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ProfileContextCollection::iterator it = profiles_.find(profile);
  if (it == profiles_.end())
    return;

  // TODO(grt): Persist incidents for upload on future profile load.

  // Forget about this profile. Incidents not yet sent for upload are lost.
  // No new incidents will be accepted for it.
  delete it->second;
  profiles_.erase(it);

  // Remove the association with this profile from any pending uploads.
  for (size_t i = 0; i < uploads_.size(); ++i) {
    UploadContext* upload = uploads_[i];
    std::vector<Profile*>::iterator it =
        std::find(upload->profiles.begin(), upload->profiles.end(), profile);
    if (it != upload->profiles.end()) {
      *it = upload->profiles.back();
      upload->profiles.resize(upload->profiles.size() - 1);
      break;
    }
  }
}

void IncidentReportingService::AddIncident(
    Profile* profile,
    scoped_ptr<ClientIncidentReport_IncidentData> incident_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Incidents outside the context of a profile are not supported at the moment.
  DCHECK(profile);

  ProfileContext* context = GetProfileContext(profile);
  // It is forbidden to call this function with a destroyed profile.
  DCHECK(context);

  // Drop the incident immediately if profile creation has completed and the
  // profile is not participating in safe browsing. Preference evaluation is
  // deferred until OnProfileAdded() if profile creation has not yet
  // completed.
  if (context->added &&
      !profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled)) {
    LogIncidentDataType(DROPPED, *incident_data);
    return;
  }

  // Start assembling a new report if this is the first incident ever or the
  // first since the last upload.
  if (!report_) {
    report_.reset(new ClientIncidentReport());
    first_incident_time_ = base::Time::Now();
  }

  // Provide time to the new incident if the caller didn't do so.
  if (!incident_data->has_incident_time_msec())
    incident_data->set_incident_time_msec(base::Time::Now().ToJavaTime());

  // Take ownership of the incident.
  context->incidents.push_back(incident_data.release());

  if (!last_incident_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SBIRS.InterIncidentTime",
                        base::TimeTicks::Now() - last_incident_time_);
  }
  last_incident_time_ = base::TimeTicks::Now();

  // Persist the incident data.

  // Restart the delay timer to send the report upon expiration.
  collection_timeout_pending_ = true;
  upload_timer_.Reset();

  BeginEnvironmentCollection();
  BeginDownloadCollection();
}

void IncidentReportingService::BeginEnvironmentCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);
  // Nothing to do if environment collection is pending or has already
  // completed.
  if (environment_collection_pending_ || report_->has_environment())
    return;

  environment_collection_begin_ = base::TimeTicks::Now();
  ClientIncidentReport_EnvironmentData* environment_data =
      new ClientIncidentReport_EnvironmentData();
  environment_collection_pending_ =
      environment_collection_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(collect_environment_data_fn_, environment_data),
          base::Bind(&IncidentReportingService::OnEnvironmentDataCollected,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(make_scoped_ptr(environment_data))));

  // Posting the task will fail if the runner has been shut down. This should
  // never happen since the blocking pool is shut down after this service.
  DCHECK(environment_collection_pending_);
}

bool IncidentReportingService::WaitingForEnvironmentCollection() {
  return environment_collection_pending_;
}

void IncidentReportingService::CancelEnvironmentCollection() {
  environment_collection_begin_ = base::TimeTicks();
  environment_collection_pending_ = false;
  if (report_)
    report_->clear_environment();
}

void IncidentReportingService::OnEnvironmentDataCollected(
    scoped_ptr<ClientIncidentReport_EnvironmentData> environment_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(environment_collection_pending_);
  DCHECK(report_ && !report_->has_environment());
  environment_collection_pending_ = false;

// CurrentProcessInfo::CreationTime() is missing on some platforms.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  base::TimeDelta uptime =
      first_incident_time_ - base::CurrentProcessInfo::CreationTime();
  environment_data->mutable_process()->set_uptime_msec(uptime.InMilliseconds());
#endif
  first_incident_time_ = base::Time();

  report_->set_allocated_environment(environment_data.release());

  UMA_HISTOGRAM_TIMES("SBIRS.EnvCollectionTime",
                      base::TimeTicks::Now() - environment_collection_begin_);
  environment_collection_begin_ = base::TimeTicks();

  UploadIfCollectionComplete();
}

bool IncidentReportingService::WaitingToCollateIncidents() {
  return collection_timeout_pending_;
}

void IncidentReportingService::CancelIncidentCollection() {
  collection_timeout_pending_ = false;
  last_incident_time_ = base::TimeTicks();
  report_.reset();
}

void IncidentReportingService::OnCollectionTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Exit early if collection was cancelled.
  if (!collection_timeout_pending_)
    return;

  // Wait another round if incidents have come in from a profile that has yet to
  // complete creation.
  for (ProfileContextCollection::iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    if (!scan->second->added && !scan->second->incidents.empty()) {
      upload_timer_.Reset();
      return;
    }
  }

  collection_timeout_pending_ = false;

  UploadIfCollectionComplete();
}

void IncidentReportingService::BeginDownloadCollection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);
  // Nothing to do if a search for the most recent download is already pending
  // or if one has already been found.
  if (last_download_finder_ || report_->has_download())
    return;

  last_download_begin_ = base::TimeTicks::Now();
  last_download_finder_ = CreateDownloadFinder(
      base::Bind(&IncidentReportingService::OnLastDownloadFound,
                 weak_ptr_factory_.GetWeakPtr()));
  // No instance is returned if there are no eligible loaded profiles. Another
  // search will be attempted in OnProfileAdded() if another profile appears on
  // the scene.
  if (!last_download_finder_)
    last_download_begin_ = base::TimeTicks();
}

bool IncidentReportingService::WaitingForMostRecentDownload() {
  DCHECK(report_);  // Only call this when a report is being assembled.
  // The easy case: not waiting if a download has already been found.
  if (report_->has_download())
    return false;
  // The next easy case: waiting if the finder is operating.
  if (last_download_finder_)
    return true;
  // The harder case: waiting if a profile has not yet been added.
  for (ProfileContextCollection::const_iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    if (!scan->second->added)
      return true;
  }
  // There is no most recent download and there's nothing more to wait for.
  return false;
}

void IncidentReportingService::CancelDownloadCollection() {
  last_download_finder_.reset();
  last_download_begin_ = base::TimeTicks();
  if (report_)
    report_->clear_download();
}

void IncidentReportingService::OnLastDownloadFound(
    scoped_ptr<ClientIncidentReport_DownloadDetails> last_download) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(report_);

  UMA_HISTOGRAM_TIMES("SBIRS.FindDownloadedBinaryTime",
                      base::TimeTicks::Now() - last_download_begin_);
  last_download_begin_ = base::TimeTicks();

  // Harvest the finder.
  last_download_finder_.reset();

  if (last_download)
    report_->set_allocated_download(last_download.release());

  UploadIfCollectionComplete();
}

void IncidentReportingService::UploadIfCollectionComplete() {
  DCHECK(report_);
  // Bail out if there are still outstanding collection tasks. Completion of any
  // of these will start another upload attempt.
  if (WaitingForEnvironmentCollection() ||
      WaitingToCollateIncidents() ||
      WaitingForMostRecentDownload()) {
    return;
  }

  // Take ownership of the report and clear things for future reports.
  scoped_ptr<ClientIncidentReport> report(report_.Pass());
  last_incident_time_ = base::TimeTicks();

  // Drop the report if no executable download was found.
  if (!report->has_download()) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.UploadResult",
                              IncidentReportUploader::UPLOAD_NO_DOWNLOAD,
                              IncidentReportUploader::NUM_UPLOAD_RESULTS);
    return;
  }

  ClientIncidentReport_EnvironmentData_Process* process =
      report->mutable_environment()->mutable_process();

  // Not all platforms have a metrics reporting preference.
  if (g_browser_process->local_state()->FindPreference(
          prefs::kMetricsReportingEnabled)) {
    process->set_metrics_consent(g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled));
  }

  // Check for extended consent in any profile while collecting incidents.
  process->set_extended_consent(false);
  // Collect incidents across all profiles participating in safe browsing. Drop
  // incidents if the profile stopped participating before collection completed.
  // Prune incidents if the profile has already submitted any incidents.
  // Associate the participating profiles with the upload.
  size_t prune_count = 0;
  std::vector<Profile*> profiles;
  for (ProfileContextCollection::iterator scan = profiles_.begin();
       scan != profiles_.end();
       ++scan) {
    PrefService* prefs = scan->first->GetPrefs();
    if (process &&
        prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled)) {
      process->set_extended_consent(true);
      process = NULL;  // Don't check any more once one is found.
    }
    ProfileContext* context = scan->second;
    if (context->incidents.empty())
      continue;
    if (!prefs->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        LogIncidentDataType(DROPPED, *context->incidents[i]);
      }
      context->incidents.clear();
    } else if (prefs->GetBoolean(prefs::kSafeBrowsingIncidentReportSent)) {
      // Prune all incidents.
      // TODO(grt): Only prune previously submitted incidents;
      // http://crbug.com/383043.
      prune_count += context->incidents.size();
      context->incidents.clear();
    } else {
      for (size_t i = 0; i < context->incidents.size(); ++i) {
        ClientIncidentReport_IncidentData* incident = context->incidents[i];
        LogIncidentDataType(ACCEPTED, *incident);
        // Ownership of the incident is passed to the report.
        report->mutable_incident()->AddAllocated(incident);
      }
      context->incidents.weak_clear();
      profiles.push_back(scan->first);
    }
  }

  const int count = report->incident_size();
  // Abandon the request if all incidents were dropped with none pruned.
  if (!count && !prune_count)
    return;

  UMA_HISTOGRAM_COUNTS_100("SBIRS.IncidentCount", count + prune_count);

  {
    double prune_pct = static_cast<double>(prune_count);
    prune_pct = prune_pct * 100.0 / (count + prune_count);
    prune_pct = round(prune_pct);
    UMA_HISTOGRAM_PERCENTAGE("SBIRS.PruneRatio", static_cast<int>(prune_pct));
  }
  // Abandon the report if all incidents were pruned.
  if (!count)
    return;

  scoped_ptr<UploadContext> context(new UploadContext(report.Pass()));
  context->profiles.swap(profiles);
  if (!database_manager_) {
    // No database manager during testing. Take ownership of the context and
    // continue processing.
    UploadContext* temp_context = context.get();
    uploads_.push_back(context.release());
    IncidentReportingService::OnKillSwitchResult(temp_context, false);
  } else {
    if (content::BrowserThread::PostTaskAndReplyWithResult(
            content::BrowserThread::IO,
            FROM_HERE,
            base::Bind(&SafeBrowsingDatabaseManager::IsCsdWhitelistKillSwitchOn,
                       database_manager_),
            base::Bind(&IncidentReportingService::OnKillSwitchResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       context.get()))) {
      uploads_.push_back(context.release());
    }  // else should not happen. Let the context be deleted automatically.
  }
}

void IncidentReportingService::CancelAllReportUploads() {
  for (size_t i = 0; i < uploads_.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.UploadResult",
                              IncidentReportUploader::UPLOAD_CANCELLED,
                              IncidentReportUploader::NUM_UPLOAD_RESULTS);
  }
  uploads_.clear();
}

void IncidentReportingService::OnKillSwitchResult(UploadContext* context,
                                                  bool is_killswitch_on) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!is_killswitch_on) {
    // Initiate the upload.
    context->uploader =
        StartReportUpload(
            base::Bind(&IncidentReportingService::OnReportUploadResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       context),
            url_request_context_getter_,
            *context->report).Pass();
    if (!context->uploader) {
      OnReportUploadResult(context,
                           IncidentReportUploader::UPLOAD_INVALID_REQUEST,
                           scoped_ptr<ClientIncidentResponse>());
    }
  } else {
    OnReportUploadResult(context,
                         IncidentReportUploader::UPLOAD_SUPPRESSED,
                         scoped_ptr<ClientIncidentResponse>());
  }
}

void IncidentReportingService::HandleResponse(const UploadContext& context) {
  for (size_t i = 0; i < context.profiles.size(); ++i) {
    context.profiles[i]->GetPrefs()->SetBoolean(
        prefs::kSafeBrowsingIncidentReportSent, true);
  }
}

void IncidentReportingService::OnReportUploadResult(
    UploadContext* context,
    IncidentReportUploader::Result result,
    scoped_ptr<ClientIncidentResponse> response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UMA_HISTOGRAM_ENUMERATION(
      "SBIRS.UploadResult", result, IncidentReportUploader::NUM_UPLOAD_RESULTS);

  // The upload is no longer outstanding, so take ownership of the context (from
  // the collection of outstanding uploads) in this scope.
  ScopedVector<UploadContext>::iterator it(
      std::find(uploads_.begin(), uploads_.end(), context));
  DCHECK(it != uploads_.end());
  scoped_ptr<UploadContext> upload(context);  // == *it
  *it = uploads_.back();
  uploads_.weak_erase(uploads_.end() - 1);

  if (result == IncidentReportUploader::UPLOAD_SUCCESS)
    HandleResponse(*upload);
  // else retry?
}

void IncidentReportingService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        OnProfileAdded(profile);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (!profile->IsOffTheRecord())
        OnProfileDestroyed(profile);
      break;
    }
    default:
      break;
  }
}

}  // namespace safe_browsing
