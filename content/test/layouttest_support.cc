// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/layouttest_support.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/public/common/page_state.h"
#include "content/renderer/compositor_bindings/web_layer_impl.h"
#include "content/renderer/history_entry.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/web_frame_test_proxy.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebBatteryStatus.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionData.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationData.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"

#if defined(OS_MACOSX)
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#endif

using blink::WebBatteryStatus;
using blink::WebDeviceMotionData;
using blink::WebDeviceOrientationData;
using blink::WebGamepad;
using blink::WebGamepads;
using blink::WebRect;
using blink::WebSize;

namespace content {

namespace {

base::LazyInstance<base::Callback<void(RenderView*, WebTestProxyBase*)> >::Leaky
    g_callback = LAZY_INSTANCE_INITIALIZER;

RenderViewImpl* CreateWebTestProxy(RenderViewImplParams* params) {
  typedef WebTestProxy<RenderViewImpl, RenderViewImplParams*> ProxyType;
  ProxyType* render_view_proxy = new ProxyType(params);
  if (g_callback == 0)
    return render_view_proxy;
  g_callback.Get().Run(render_view_proxy, render_view_proxy);
  return render_view_proxy;
}

WebTestProxyBase* GetWebTestProxyBase(RenderViewImpl* render_view) {
  typedef WebTestProxy<RenderViewImpl, RenderViewImplParams*> ViewProxy;

  ViewProxy* render_view_proxy = static_cast<ViewProxy*>(render_view);
  return static_cast<WebTestProxyBase*>(render_view_proxy);
}

RenderFrameImpl* CreateWebFrameTestProxy(
    RenderViewImpl* render_view,
    int32 routing_id) {
  typedef WebFrameTestProxy<RenderFrameImpl, RenderViewImpl*, int32> FrameProxy;

  FrameProxy* render_frame_proxy = new FrameProxy(render_view, routing_id);
  render_frame_proxy->set_base_proxy(GetWebTestProxyBase(render_view));

  return render_frame_proxy;
}

}  // namespace


void EnableWebTestProxyCreation(
    const base::Callback<void(RenderView*, WebTestProxyBase*)>& callback) {
  g_callback.Get() = callback;
  RenderViewImpl::InstallCreateHook(CreateWebTestProxy);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void SetMockGamepadProvider(RendererGamepadProvider* provider) {
  RenderThreadImpl::current()->webkit_platform_support()->
      set_gamepad_provider(provider);
}

void SetMockDeviceLightData(const double data) {
  RendererWebKitPlatformSupportImpl::SetMockDeviceLightDataForTesting(data);
}

void SetMockDeviceMotionData(const WebDeviceMotionData& data) {
  RendererWebKitPlatformSupportImpl::SetMockDeviceMotionDataForTesting(data);
}

void SetMockDeviceOrientationData(const WebDeviceOrientationData& data) {
  RendererWebKitPlatformSupportImpl::
      SetMockDeviceOrientationDataForTesting(data);
}

void MockBatteryStatusChanged(const WebBatteryStatus& status) {
  RendererWebKitPlatformSupportImpl::MockBatteryStatusChangedForTesting(status);
}

void EnableRendererLayoutTestMode() {
  RenderThreadImpl::current()->set_layout_test_mode(true);
}

void EnableBrowserLayoutTestMode() {
#if defined(OS_MACOSX)
  ImageTransportSurface::SetAllowOSMesaForTesting(true);
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)->
      GetLocalSessionHistoryLengthForTesting();
}

void SyncNavigationState(RenderView* render_view) {
  static_cast<RenderViewImpl*>(render_view)->SyncNavigationState();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view,
                           const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  render_view_impl->ForceResizeForTesting(new_size);
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  static_cast<RenderViewImpl*>(render_view)->
      SetDeviceScaleFactorForTesting(factor);
}

void SetDeviceColorProfile(RenderView* render_view, const std::string& name) {
  std::vector<char> color_profile;

  struct TestColorProfile {
    char* data() {
      static unsigned char color_profile_data[] = {
        0x00,0x00,0x01,0xea,0x54,0x45,0x53,0x54,0x00,0x00,0x00,0x00,
        0x6d,0x6e,0x74,0x72,0x52,0x47,0x42,0x20,0x58,0x59,0x5a,0x20,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x61,0x63,0x73,0x70,0x74,0x65,0x73,0x74,0x00,0x00,0x00,0x00,
        0x74,0x65,0x73,0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf6,0xd6,
        0x00,0x01,0x00,0x00,0x00,0x00,0xd3,0x2d,0x74,0x65,0x73,0x74,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x09,
        0x63,0x70,0x72,0x74,0x00,0x00,0x00,0xf0,0x00,0x00,0x00,0x0d,
        0x64,0x65,0x73,0x63,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x8c,
        0x77,0x74,0x70,0x74,0x00,0x00,0x01,0x8c,0x00,0x00,0x00,0x14,
        0x72,0x58,0x59,0x5a,0x00,0x00,0x01,0xa0,0x00,0x00,0x00,0x14,
        0x67,0x58,0x59,0x5a,0x00,0x00,0x01,0xb4,0x00,0x00,0x00,0x14,
        0x62,0x58,0x59,0x5a,0x00,0x00,0x01,0xc8,0x00,0x00,0x00,0x14,
        0x72,0x54,0x52,0x43,0x00,0x00,0x01,0xdc,0x00,0x00,0x00,0x0e,
        0x67,0x54,0x52,0x43,0x00,0x00,0x01,0xdc,0x00,0x00,0x00,0x0e,
        0x62,0x54,0x52,0x43,0x00,0x00,0x01,0xdc,0x00,0x00,0x00,0x0e,
        0x74,0x65,0x78,0x74,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x64,0x65,0x73,0x63,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x10,0x77,0x68,0x61,0x63,0x6b,0x65,0x64,0x2e,
        0x69,0x63,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x11,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0xf3,0x52,
        0x00,0x01,0x00,0x00,0x00,0x01,0x16,0xcc,0x58,0x59,0x5a,0x20,
        0x00,0x00,0x00,0x00,0x00,0x00,0x34,0x8d,0x00,0x00,0xa0,0x2c,
        0x00,0x00,0x0f,0x95,0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,
        0x00,0x00,0x26,0x31,0x00,0x00,0x10,0x2f,0x00,0x00,0xbe,0x9b,
        0x58,0x59,0x5a,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x9c,0x18,
        0x00,0x00,0x4f,0xa5,0x00,0x00,0x04,0xfc,0x63,0x75,0x72,0x76,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x33
      };

      return reinterpret_cast<char*>(color_profile_data);
    }

    size_t size() {
      const size_t kColorProfileSizeInBytes = 490u;
      return kColorProfileSizeInBytes;
    }
  };

  if (name == "sRGB") {
    color_profile.assign(name.data(), name.data() + name.size());
  } else if (name == "test") {
    TestColorProfile test;
    color_profile.assign(test.data(), test.data() + test.size());
  }

  static_cast<RenderViewImpl*>(render_view)->
      SetDeviceColorProfileForTesting(color_profile);
}

void UseSynchronousResizeMode(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  static_cast<RenderViewImpl*>(render_view)->
      EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  static_cast<RenderViewImpl*>(render_view)->
      DisableAutoResizeForTesting(new_size);
}

struct ToLower {
  base::char16 operator()(base::char16 c) { return tolower(c); }
};

// Returns True if node1 < node2.
bool HistoryEntryCompareLess(HistoryEntry::HistoryNode* node1,
                             HistoryEntry::HistoryNode* node2) {
  base::string16 target1 = node1->item().target();
  base::string16 target2 = node2->item().target();
  std::transform(target1.begin(), target1.end(), target1.begin(), ToLower());
  std::transform(target2.begin(), target2.end(), target2.begin(), ToLower());
  return target1 < target2;
}

std::string DumpHistoryItem(HistoryEntry::HistoryNode* node,
                            int indent,
                            bool is_current_index) {
  std::string result;

  const blink::WebHistoryItem& item = node->item();
  if (is_current_index) {
    result.append("curr->");
    result.append(indent - 6, ' '); // 6 == "curr->".length()
  } else {
    result.append(indent, ' ');
  }

  std::string url = normalizeLayoutTestURL(item.urlString().utf8());
  result.append(url);
  if (!item.target().isEmpty()) {
    result.append(" (in frame \"");
    result.append(item.target().utf8());
    result.append("\")");
  }
  result.append("\n");

  std::vector<HistoryEntry::HistoryNode*> children = node->children();
  if (!children.empty()) {
    std::sort(children.begin(), children.end(), HistoryEntryCompareLess);
    for (size_t i = 0; i < children.size(); ++i)
      result += DumpHistoryItem(children[i], indent + 4, false);
  }

  return result;
}

std::string DumpBackForwardList(std::vector<PageState>& page_state,
                                size_t current_index) {
  std::string result;
  result.append("\n============== Back Forward List ==============\n");
  for (size_t index = 0; index < page_state.size(); ++index) {
    scoped_ptr<HistoryEntry> entry(
        PageStateToHistoryEntry(page_state[index]));
    result.append(
        DumpHistoryItem(entry->root_history_node(),
                        8,
                        index == current_index));
  }
  result.append("===============================================\n");
  return result;
}

blink::WebLayer* InstantiateWebLayer(scoped_refptr<cc::TextureLayer> layer) {
  return new WebLayerImpl(layer);
}

}  // namespace content
