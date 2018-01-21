// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/context_menu_content_type.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"


using blink::WebContextMenuData;
using content::WebContents;
using extensions::Extension;

namespace {

bool IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

bool IsInternalResourcesURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
      (url.host() == chrome::kChromeUISyncResourcesHost);
}

}  // namespace

ContextMenuContentType::ContextMenuContentType(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params,
    bool supports_custom_items)
    : params_(params),
      source_web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(
                   source_web_contents_->GetBrowserContext())),
      supports_custom_items_(supports_custom_items) {
}

ContextMenuContentType::~ContextMenuContentType() {
}

const Extension* ContextMenuContentType::GetExtension() const {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  // There is no process manager in some tests.
  if (!system->process_manager())
    return NULL;

  return system->process_manager()->GetExtensionForRenderViewHost(
      source_web_contents_->GetRenderViewHost());
}

bool ContextMenuContentType::SupportsGroup(int group) {
  const bool has_selection = !params_.selection_text.empty();

  if (supports_custom_items_ && !params_.custom_items.empty()) {
    if (group == ITEM_GROUP_CUSTOM)
      return true;

    if (!has_selection) {
      // For menus with custom items, if there is no selection, we do not
      // add items other than developer items. And for Pepper menu, don't even
      // add developer items.
      if (!params_.custom_context.is_pepper_menu)
        return group == ITEM_GROUP_DEVELOPER;

      return false;
    }

    // If there's a selection when there are custom items, fall through to
    // adding the normal ones after the custom ones.
  }

  return SupportsGroupInternal(group);
}

bool ContextMenuContentType::SupportsGroupInternal(int group) {
  const bool has_link = !params_.unfiltered_link_url.is_empty();
  const bool has_selection = !params_.selection_text.empty();

  switch (group) {
    case ITEM_GROUP_CUSTOM:
      return supports_custom_items_ && !params_.custom_items.empty();

    case ITEM_GROUP_PAGE: {
      bool is_candidate =
          params_.media_type == WebContextMenuData::MediaTypeNone &&
          !has_link && !params_.is_editable && !has_selection;

      if (!is_candidate && params_.page_url.is_empty())
        DCHECK(params_.frame_url.is_empty());

      return is_candidate && !params_.page_url.is_empty() &&
          !IsDevToolsURL(params_.page_url) &&
          !IsInternalResourcesURL(params_.page_url);
    }

    case ITEM_GROUP_FRAME: {

      bool page_group_supported = SupportsGroupInternal(ITEM_GROUP_PAGE);
      return page_group_supported && !params_.frame_url.is_empty() &&
          !IsDevToolsURL(params_.frame_url) &&
          !IsInternalResourcesURL(params_.page_url);
    }

    case ITEM_GROUP_LINK:
      return has_link;

    case ITEM_GROUP_MEDIA_IMAGE:
      return params_.media_type == WebContextMenuData::MediaTypeImage;

    case ITEM_GROUP_SEARCHWEBFORIMAGE:
      // Image menu items imply search web for image item.
      return SupportsGroupInternal(ITEM_GROUP_MEDIA_IMAGE);

    case ITEM_GROUP_MEDIA_VIDEO:
      return params_.media_type == WebContextMenuData::MediaTypeVideo;

    case ITEM_GROUP_MEDIA_AUDIO:
      return params_.media_type == WebContextMenuData::MediaTypeAudio;

    case ITEM_GROUP_MEDIA_CANVAS:
      return params_.media_type == WebContextMenuData::MediaTypeCanvas;

    case ITEM_GROUP_MEDIA_PLUGIN:
      return params_.media_type == WebContextMenuData::MediaTypePlugin;

    case ITEM_GROUP_MEDIA_FILE:
#if defined(WEBCONTEXT_MEDIATYPEFILE_DEFINED)
      return params_.media_type == WebContextMenuData::MediaTypeFile;
#else
      return false;
#endif

    case ITEM_GROUP_EDITABLE:
      return params_.is_editable;

    case ITEM_GROUP_COPY:
      return !params_.is_editable && has_selection;

    case ITEM_GROUP_SEARCH_PROVIDER:
      return has_selection;

    case ITEM_GROUP_PRINT: {
      bool enable = has_selection && !IsDevToolsURL(params_.page_url);
      // Image menu items also imply print items.
      return enable || SupportsGroupInternal(ITEM_GROUP_MEDIA_IMAGE);
    }

    case ITEM_GROUP_ALL_EXTENSION:
      return !IsDevToolsURL(params_.page_url);

    case ITEM_GROUP_CURRENT_EXTENSION:
      return false;

    case ITEM_GROUP_DEVELOPER:
      return true;

    case ITEM_GROUP_DEVTOOLS_UNPACKED_EXT:
      return false;

    case ITEM_GROUP_PRINT_PREVIEW:
#if defined(ENABLE_FULL_PRINTING)
      return true;
#else
      return false;
#endif

    default:
      NOTREACHED();
      return false;
  }
}
