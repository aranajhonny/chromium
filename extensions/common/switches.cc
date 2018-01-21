// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/switches.h"

namespace extensions {

namespace switches {

// Allows non-https URL for background_page for hosted apps.
const char kAllowHTTPBackgroundPage[] = "allow-http-background-page";

// Allows the browser to load extensions that lack a modern manifest when that
// would otherwise be forbidden.
const char kAllowLegacyExtensionManifests[] =
    "allow-legacy-extension-manifests";

// Enables extension options to be embedded in chrome://extensions rather than
// a new tab.
const char kEmbeddedExtensionOptions[] = "embedded-extension-options";

// Hack so that feature switch can work with about_flags. See
// kEnableScriptsRequireAction.
const char kEnableEmbeddedExtensionOptions[] =
    "enable-embedded-extension-options";

// Enables extension APIs that are in development.
const char kEnableExperimentalExtensionApis[] =
    "enable-experimental-extension-apis";

// Enables extensions to hide bookmarks UI elements.
const char kEnableOverrideBookmarksUI[] = "enable-override-bookmarks-ui";

// Allows the ErrorConsole to collect runtime and manifest errors, and display
// them in the chrome:extensions page.
const char kErrorConsole[] = "error-console";

// The time in milliseconds that an extension event page can be idle before it
// is shut down.
const char kEventPageIdleTime[] = "event-page-idle-time";

// The time in milliseconds that an extension event page has between being
// notified of its impending unload and that unload happening.
const char kEventPageSuspendingTime[] = "event-page-unloading-time";

// Whether to switch to extension action redesign mode (experimental).
const char kExtensionActionRedesign[] = "extension-action-redesign";

// Marks a renderer as extension process.
const char kExtensionProcess[] = "extension-process";

// Enables extensions running scripts on chrome:// URLs.
// Extensions still need to explicitly request access to chrome:// URLs in the
// manifest.
const char kExtensionsOnChromeURLs[] = "extensions-on-chrome-urls";

// Whether to force developer mode extensions highlighting.
const char kForceDevModeHighlighting[] = "force-dev-mode-highlighting";

// Notify the user and require consent for extensions running scripts.
// Appending --scripts-require-action=1 has the same effect as
// --enable-scripts-require-action (see below).
const char kScriptsRequireAction[] = "scripts-require-action";
// FeatureSwitch and about_flags don't play nice. Feature switch expects either
// --enable-<feature> or --<feature>=1, but about_flags expects the command
// line argument to enable it (or a selection). Hack this in, so enabling it
// in about_flags enables the feature. Appending this flag has the same effect
// as --scripts-require-action=1.
const char kEnableScriptsRequireAction[] = "enable-scripts-require-action";

// Makes component extensions appear in chrome://settings/extensions.
const char kShowComponentExtensionOptions[] =
    "show-component-extension-options";

// Adds the given extension ID to all the permission whitelists.
const char kWhitelistedExtensionID[] = "whitelisted-extension-id";

}  // namespace switches

}  // namespace extensions
