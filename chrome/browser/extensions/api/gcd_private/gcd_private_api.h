// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace extensions {

class GcdPrivateAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer,
                      public local_discovery::PrivetDeviceLister::Delegate {
 public:
  class GCDApiFlowFactoryForTests {
   public:
    virtual ~GCDApiFlowFactoryForTests() {}

    virtual scoped_ptr<local_discovery::GCDApiFlow> CreateGCDApiFlow() = 0;
  };

  explicit GcdPrivateAPI(content::BrowserContext* context);
  virtual ~GcdPrivateAPI();

  static void SetGCDApiFlowFactoryForTests(GCDApiFlowFactoryForTests* factory);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<GcdPrivateAPI>* GetFactoryInstance();

  bool QueryForDevices();

 private:
  friend class BrowserContextKeyedAPIFactory<GcdPrivateAPI>;

  typedef std::map<std::string /* id_string */,
                   linked_ptr<api::gcd_private::GCDDevice> > GCDDeviceMap;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "GcdPrivateAPI"; }

  // local_discovery::PrivetDeviceLister implementation.
  virtual void DeviceChanged(
      bool added,
      const std::string& name,
      const local_discovery::DeviceDescription& description) OVERRIDE;
  virtual void DeviceRemoved(const std::string& name) OVERRIDE;
  virtual void DeviceCacheFlushed() OVERRIDE;

  int num_device_listeners_;
  scoped_refptr<local_discovery::ServiceDiscoverySharedClient>
      service_discovery_client_;
  scoped_ptr<local_discovery::PrivetDeviceLister> privet_device_lister_;
  GCDDeviceMap known_devices_;

  content::BrowserContext* const browser_context_;
};

class GcdPrivateGetCloudDeviceListFunction
    : public ChromeAsyncExtensionFunction,
      public local_discovery::CloudDeviceListDelegate {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCloudDeviceList",
                             GCDPRIVATE_GETCLOUDDEVICELIST)

  GcdPrivateGetCloudDeviceListFunction();

 protected:
  virtual ~GcdPrivateGetCloudDeviceListFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // CloudDeviceListDelegate implementation
  virtual void OnDeviceListReady(const DeviceList& devices) OVERRIDE;
  virtual void OnDeviceListUnavailable() OVERRIDE;

  void CheckListingDone();

  int requests_succeeded_;
  int requests_failed_;
  DeviceList devices_;

  scoped_ptr<local_discovery::GCDApiFlow> printer_list_;
  scoped_ptr<local_discovery::GCDApiFlow> device_list_;
};

class GcdPrivateQueryForNewLocalDevicesFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.queryForNewLocalDevices",
                             GCDPRIVATE_QUERYFORNEWLOCALDEVICES)

  GcdPrivateQueryForNewLocalDevicesFunction();

 protected:
  virtual ~GcdPrivateQueryForNewLocalDevicesFunction();

  // SyncExtensionFunction overrides.
  virtual bool RunSync() OVERRIDE;
};

class GcdPrivatePrefetchWifiPasswordFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.prefetchWifiPassword",
                             GCDPRIVATE_PREFETCHWIFIPASSWORD)

  GcdPrivatePrefetchWifiPasswordFunction();

 protected:
  virtual ~GcdPrivatePrefetchWifiPasswordFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class GcdPrivateEstablishSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.establishSession",
                             GCDPRIVATE_ESTABLISHSESSION)

  GcdPrivateEstablishSessionFunction();

 protected:
  virtual ~GcdPrivateEstablishSessionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class GcdPrivateConfirmCodeFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.confirmCode", GCDPRIVATE_CONFIRMCODE)

  GcdPrivateConfirmCodeFunction();

 protected:
  virtual ~GcdPrivateConfirmCodeFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateSendMessageFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.sendMessage", GCDPRIVATE_SENDMESSAGE)

  GcdPrivateSendMessageFunction();

 protected:
  virtual ~GcdPrivateSendMessageFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateTerminateSessionFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.terminateSession",
                             GCDPRIVATE_TERMINATESESSION)

  GcdPrivateTerminateSessionFunction();

 protected:
  virtual ~GcdPrivateTerminateSessionFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

class GcdPrivateGetCommandDefinitionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommandDefinitions",
                             GCDPRIVATE_GETCOMMANDDEFINITIONS)

  GcdPrivateGetCommandDefinitionsFunction();

 protected:
  virtual ~GcdPrivateGetCommandDefinitionsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateInsertCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.insertCommand",
                             GCDPRIVATE_INSERTCOMMAND)

  GcdPrivateInsertCommandFunction();

 protected:
  virtual ~GcdPrivateInsertCommandFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateGetCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommand", GCDPRIVATE_GETCOMMAND)

  GcdPrivateGetCommandFunction();

 protected:
  virtual ~GcdPrivateGetCommandFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateCancelCommandFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.cancelCommand",
                             GCDPRIVATE_CANCELCOMMAND)

  GcdPrivateCancelCommandFunction();

 protected:
  virtual ~GcdPrivateCancelCommandFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

class GcdPrivateGetCommandsListFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("gcdPrivate.getCommandsList",
                             GCDPRIVATE_GETCOMMANDSLIST)

  GcdPrivateGetCommandsListFunction();

 protected:
  virtual ~GcdPrivateGetCommandsListFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_GCD_PRIVATE_API_H_
