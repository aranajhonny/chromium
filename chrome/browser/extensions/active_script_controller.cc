// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_script_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

namespace {

// Returns true if the extension should be regarded as a "permitted" extension
// for the case of metrics. We need this because we only actually withhold
// permissions if the switch is enabled, but want to record metrics in all
// cases.
// "ExtensionWouldHaveHadHostPermissionsWithheldIfSwitchWasOn()" would be
// more accurate, but too long.
bool ShouldRecordExtension(const Extension* extension) {
  return extension->ShouldDisplayInExtensionSettings() &&
         !Manifest::IsPolicyLocation(extension->location()) &&
         !Manifest::IsComponentLocation(extension->location()) &&
         !PermissionsData::CanExecuteScriptEverywhere(extension) &&
         extension->permissions_data()
             ->active_permissions()
             ->ShouldWarnAllHosts();
}

}  // namespace

ActiveScriptController::ActiveScriptController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      enabled_(FeatureSwitch::scripts_require_action()->IsEnabled()) {
  CHECK(web_contents);
}

ActiveScriptController::~ActiveScriptController() {
  LogUMA();
}

// static
ActiveScriptController* ActiveScriptController::GetForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return NULL;
  LocationBarController* location_bar_controller =
      tab_helper->location_bar_controller();
  // This should never be NULL.
  DCHECK(location_bar_controller);
  return location_bar_controller->active_script_controller();
}

void ActiveScriptController::OnActiveTabPermissionGranted(
    const Extension* extension) {
  RunPendingForExtension(extension);
}

void ActiveScriptController::OnAdInjectionDetected(
    const std::set<std::string>& ad_injectors) {
  // We're only interested in data if there are ad injectors detected.
  if (ad_injectors.empty())
    return;

  size_t num_preventable_ad_injectors =
      base::STLSetIntersection<std::set<std::string> >(
          ad_injectors, permitted_extensions_).size();

  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ActiveScriptController.PreventableAdInjectors",
      num_preventable_ad_injectors);
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ActiveScriptController.UnpreventableAdInjectors",
      ad_injectors.size() - num_preventable_ad_injectors);
}

ExtensionAction* ActiveScriptController::GetActionForExtension(
    const Extension* extension) {
  if (!enabled_ || pending_requests_.count(extension->id()) == 0)
    return NULL;  // No action for this extension.

  ActiveScriptMap::iterator existing =
      active_script_actions_.find(extension->id());
  if (existing != active_script_actions_.end())
    return existing->second.get();

  linked_ptr<ExtensionAction> action(new ExtensionAction(
      extension->id(), ActionInfo::TYPE_PAGE, ActionInfo()));
  action->SetTitle(ExtensionAction::kDefaultTabId, extension->name());
  action->SetIsVisible(ExtensionAction::kDefaultTabId, true);

  const ActionInfo* action_info = ActionInfo::GetPageActionInfo(extension);
  if (!action_info)
    action_info = ActionInfo::GetBrowserActionInfo(extension);

  if (action_info && !action_info->default_icon.empty()) {
    action->set_default_icon(
        make_scoped_ptr(new ExtensionIconSet(action_info->default_icon)));
  }

  active_script_actions_[extension->id()] = action;
  return action.get();
}

LocationBarController::Action ActiveScriptController::OnClicked(
    const Extension* extension) {
  DCHECK(ContainsKey(pending_requests_, extension->id()));
  RunPendingForExtension(extension);
  return LocationBarController::ACTION_NONE;
}

void ActiveScriptController::OnNavigated() {
  LogUMA();
  permitted_extensions_.clear();
  pending_requests_.clear();
}

void ActiveScriptController::OnExtensionUnloaded(const Extension* extension) {
  PendingRequestMap::iterator iter = pending_requests_.find(extension->id());
  if (iter != pending_requests_.end())
    pending_requests_.erase(iter);
}

PermissionsData::AccessType
ActiveScriptController::RequiresUserConsentForScriptInjection(
    const Extension* extension,
    UserScript::InjectionType type) {
  CHECK(extension);

  // If the feature is not enabled, we automatically allow all extensions to
  // run scripts.
  if (!enabled_)
    permitted_extensions_.insert(extension->id());

  // Allow the extension if it's been explicitly granted permission.
  if (permitted_extensions_.count(extension->id()) > 0)
    return PermissionsData::ACCESS_ALLOWED;

  GURL url = web_contents()->GetVisibleURL();
  int tab_id = SessionID::IdForTab(web_contents());
  switch (type) {
    case UserScript::CONTENT_SCRIPT:
      return extension->permissions_data()->GetContentScriptAccess(
          extension, url, url, tab_id, -1, NULL);
    case UserScript::PROGRAMMATIC_SCRIPT:
      return extension->permissions_data()->GetPageAccess(
          extension, url, url, tab_id, -1, NULL);
  }

  NOTREACHED();
  return PermissionsData::ACCESS_DENIED;
}

void ActiveScriptController::RequestScriptInjection(
    const Extension* extension,
    const base::Closure& callback) {
  CHECK(extension);
  PendingRequestList& list = pending_requests_[extension->id()];
  list.push_back(callback);

  // If this was the first entry, notify the location bar that there's a new
  // icon.
  if (list.size() == 1u)
    LocationBarController::NotifyChange(web_contents());
}

void ActiveScriptController::RunPendingForExtension(
    const Extension* extension) {
  DCHECK(extension);

  content::NavigationEntry* visible_entry =
      web_contents()->GetController().GetVisibleEntry();
  // Refuse to run if there's no visible entry, because we have no idea of
  // determining if it's the proper page. This should rarely, if ever, happen.
  if (!visible_entry)
    return;

  // We add this to the list of permitted extensions and erase pending entries
  // *before* running them to guard against the crazy case where running the
  // callbacks adds more entries.
  permitted_extensions_.insert(extension->id());

  PendingRequestMap::iterator iter = pending_requests_.find(extension->id());
  if (iter == pending_requests_.end())
    return;

  PendingRequestList requests;
  iter->second.swap(requests);
  pending_requests_.erase(extension->id());

  // Clicking to run the extension counts as granting it permission to run on
  // the given tab.
  // The extension may already have active tab at this point, but granting
  // it twice is essentially a no-op.
  TabHelper::FromWebContents(web_contents())->
      active_tab_permission_granter()->GrantIfRequested(extension);

  // Run all pending injections for the given extension.
  for (PendingRequestList::iterator request = requests.begin();
       request != requests.end();
       ++request) {
    request->Run();
  }

  // Inform the location bar that the action is now gone.
  LocationBarController::NotifyChange(web_contents());
}

void ActiveScriptController::OnRequestScriptInjectionPermission(
    const std::string& extension_id,
    UserScript::InjectionType script_type,
    int64 request_id) {
  if (!Extension::IdIsValid(extension_id)) {
    NOTREACHED() << "'" << extension_id << "' is not a valid id.";
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext())
          ->enabled_extensions().GetByID(extension_id);
  // We shouldn't allow extensions which are no longer enabled to run any
  // scripts. Ignore the request.
  if (!extension)
    return;

  // If the request id is -1, that signals that the content script has already
  // ran (because this feature is not enabled). Add the extension to the list of
  // permitted extensions (for metrics), and return immediately.
  if (request_id == -1) {
    if (ShouldRecordExtension(extension)) {
      DCHECK(!enabled_);
      permitted_extensions_.insert(extension->id());
    }
    return;
  }

  switch (RequiresUserConsentForScriptInjection(extension, script_type)) {
    case PermissionsData::ACCESS_ALLOWED:
      PermitScriptInjection(request_id);
      break;
    case PermissionsData::ACCESS_WITHHELD:
      // This base::Unretained() is safe, because the callback is only invoked
      // by this object.
      RequestScriptInjection(
          extension,
          base::Bind(&ActiveScriptController::PermitScriptInjection,
                     base::Unretained(this),
                     request_id));
      break;
    case PermissionsData::ACCESS_DENIED:
      // We should usually only get a "deny access" if the page changed (as the
      // renderer wouldn't have requested permission if the answer was always
      // "no"). Just let the request fizzle and die.
      break;
  }
}

void ActiveScriptController::PermitScriptInjection(int64 request_id) {
  // This only sends the response to the renderer - the process of adding the
  // extension to the list of |permitted_extensions_| is done elsewhere.
  content::RenderViewHost* render_view_host =
      web_contents()->GetRenderViewHost();
  if (render_view_host) {
    render_view_host->Send(new ExtensionMsg_PermitScriptInjection(
        render_view_host->GetRoutingID(), request_id));
  }
}

bool ActiveScriptController::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ActiveScriptController, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestScriptInjectionPermission,
                        OnRequestScriptInjectionPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ActiveScriptController::LogUMA() const {
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ActiveScriptController.ShownActiveScriptsOnPage",
      pending_requests_.size());

  // We only log the permitted extensions metric if the feature is enabled,
  // because otherwise the data will be boring (100% allowed).
  if (enabled_) {
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.PermittedExtensions",
        permitted_extensions_.size());
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.DeniedExtensions",
        pending_requests_.size());
  }
}

}  // namespace extensions
