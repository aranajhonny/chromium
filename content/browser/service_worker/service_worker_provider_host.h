// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <set>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"

namespace IPC {
class Sender;
}

namespace webkit_blob {
class BlobStorageContext;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerDispatcherHost;
class ServiceWorkerRequestHandler;
class ServiceWorkerVersion;

// This class is the browser-process representation of a service worker
// provider. There is a provider per document and the lifetime of this
// object is tied to the lifetime of its document in the renderer process.
// This class holds service worker state that is scoped to an individual
// document.
//
// Note this class can also host a running service worker, in which
// case it will observe resource loads made directly by the service worker.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public base::SupportsWeakPtr<ServiceWorkerProviderHost> {
 public:
  ServiceWorkerProviderHost(int process_id,
                            int provider_id,
                            base::WeakPtr<ServiceWorkerContextCore> context,
                            ServiceWorkerDispatcherHost* dispatcher_host);
  ~ServiceWorkerProviderHost();

  int process_id() const { return process_id_; }
  int provider_id() const { return provider_id_; }

  bool IsHostToRunningServiceWorker() {
    return running_hosted_version_ != NULL;
  }

  // Getters for the navigator.serviceWorker attribute values.
  ServiceWorkerVersion* controlling_version() const {
    return controlling_version_.get();
  }
  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }
  ServiceWorkerVersion* waiting_version() const {
    return waiting_version_.get();
  }
  ServiceWorkerVersion* installing_version() const {
    return installing_version_.get();
  }

  // The running version, if any, that this provider is providing resource
  // loads for.
  ServiceWorkerVersion* running_hosted_version() const {
    return running_hosted_version_.get();
  }

  void SetDocumentUrl(const GURL& url);
  const GURL& document_url() const { return document_url_; }

  // Associates |version| to the corresponding field or if |version| is NULL
  // clears the field.
  void SetControllerVersion(ServiceWorkerVersion* version);
  void SetActiveVersion(ServiceWorkerVersion* version);
  void SetWaitingVersion(ServiceWorkerVersion* version);
  void SetInstallingVersion(ServiceWorkerVersion* version);

  // If |version| is the installing, waiting, or active version of this
  // provider, the method will reset that field to NULL.
  void UnsetVersion(ServiceWorkerVersion* version);

  // Returns false if the version is not in the expected STARTING in our
  // process state. That would be indicative of a bad IPC message.
  bool SetHostedVersionId(int64 versions_id);

  // Returns a handler for a request, the handler may return NULL if
  // the request doesn't require special handling.
  scoped_ptr<ServiceWorkerRequestHandler> CreateRequestHandler(
      ResourceType::Type resource_type,
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_storage_context);

  // Returns true if |version| can be associated with this provider.
  bool CanAssociateVersion(ServiceWorkerVersion* version);

  // Returns true if the context referred to by this host (i.e. |context_|) is
  // still alive.
  bool IsContextAlive();

  // Dispatches message event to the document.
  void PostMessage(const base::string16& message,
                   const std::vector<int>& sent_message_port_ids);

 private:
  // Creates a ServiceWorkerHandle to retain |version| and returns a
  // ServiceWorkerInfo with the handle ID to pass to the provider. The
  // provider is responsible for releasing the handle.
  ServiceWorkerObjectInfo CreateHandleAndPass(ServiceWorkerVersion* version);

  const int process_id_;
  const int provider_id_;
  GURL document_url_;

  scoped_refptr<ServiceWorkerVersion> controlling_version_;
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;
  scoped_refptr<ServiceWorkerVersion> installing_version_;
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  ServiceWorkerDispatcherHost* dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
