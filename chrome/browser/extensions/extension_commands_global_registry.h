// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "ui/base/accelerators/accelerator.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;

// ExtensionCommandsGlobalRegistry is a class that handles the cross-platform
// implementation of the global shortcut registration for the Extension Commands
// API).
// Note: It handles regular extension commands (not browserAction and pageAction
// popups, which are not bindable to global shortcuts). This class registers the
// accelerators on behalf of the extensions and routes the commands to them via
// the BrowserEventRouter.
class ExtensionCommandsGlobalRegistry
    : public BrowserContextKeyedAPI,
      public ExtensionKeybindingRegistry,
      public GlobalShortcutListener::Observer {
 public:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>*
      GetFactoryInstance();

  // Convenience method to get the ExtensionCommandsGlobalRegistry for a
  // profile.
  static ExtensionCommandsGlobalRegistry* Get(content::BrowserContext* context);

  // Enables/Disables global shortcut handling in Chrome.
  static void SetShortcutHandlingSuspended(bool suspended);

  explicit ExtensionCommandsGlobalRegistry(content::BrowserContext* context);
  virtual ~ExtensionCommandsGlobalRegistry();

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionCommandsGlobalRegistry>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "ExtensionCommandsGlobalRegistry";
  }

  // Overridden from ExtensionKeybindingRegistry:
  virtual void AddExtensionKeybinding(
      const Extension* extension,
      const std::string& command_name) OVERRIDE;
  virtual void RemoveExtensionKeybindingImpl(
      const ui::Accelerator& accelerator,
      const std::string& command_name) OVERRIDE;

  // Called by the GlobalShortcutListener object when a shortcut this class has
  // registered for has been pressed.
  virtual void OnKeyPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // Weak pointer to our browser context. Not owned by us.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCommandsGlobalRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COMMANDS_GLOBAL_REGISTRY_H_
