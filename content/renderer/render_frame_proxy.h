// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
#define CONTENT_RENDERER_RENDER_FRAME_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"

struct FrameMsg_BuffersSwapped_Params;
struct FrameMsg_CompositorFrameSwapped_Params;

namespace content {

class ChildFrameCompositingHelper;
class RenderFrameImpl;
class RenderViewImpl;

// When a page's frames are rendered by multiple processes, each renderer has a
// full copy of the frame tree. It has full RenderFrames for the frames it is
// responsible for rendering and placeholder objects for frames rendered by
// other processes. This class is the renderer-side object for the placeholder.
// RenderFrameProxy allows us to keep existing window references valid over
// cross-process navigations and route cross-site asynchronous JavaScript calls,
// such as postMessage.
//
// For now, RenderFrameProxy is created when RenderFrame is swapped out. It
// acts as a wrapper and is used for sending and receiving IPC messages. It is
// deleted when the RenderFrame is swapped back in or the node of the frame
// tree is deleted.
//
// Long term, RenderFrameProxy will be created to replace the RenderFrame in the
// frame tree and the RenderFrame will be deleted after its unload handler has
// finished executing. It will still be responsible for routing IPC messages
// which are valid for cross-site interactions between frames.
// RenderFrameProxy will be deleted when the node in the frame tree is deleted
// or when navigating the frame causes it to return to this process and a new
// RenderFrame is created for it.
class CONTENT_EXPORT RenderFrameProxy
    : public IPC::Listener,
      public IPC::Sender,
      NON_EXPORTED_BASE(public blink::WebFrameClient) {
 public:
  // This method should be used to create a RenderFrameProxy, which will replace
  // an existing RenderFrame during its cross-process navigation from the
  // current process to a different one. |routing_id| will be ID of the newly
  // created RenderFrameProxy. |frame_to_replace| is the frame that the new
  // proxy will eventually swap places with.
  static RenderFrameProxy* CreateProxyToReplaceFrame(
      RenderFrameImpl* frame_to_replace,
      int routing_id);

  // This method should be used to create a RenderFrameProxy, when there isn't
  // an existing RenderFrame. It should be called to construct a local
  // representation of a RenderFrame that has been created in another process --
  // for example, after a cross-process navigation or after the addition of a
  // new frame local to some other process. |routing_id| will be the ID of the
  // newly created RenderFrameProxy. |parent_routing_id| is the routing ID of
  // the RenderFrameProxy to which the new frame is parented.
  // |render_view_routing_id| identifies the RenderView to be associated with
  // this frame.
  //
  // |parent_routing_id| always identifies a RenderFrameProxy (never a
  // RenderFrame) because a new child of a local frame should always start out
  // as a frame, not a proxy.
  static RenderFrameProxy* CreateFrameProxy(int routing_id,
                                            int parent_routing_id,
                                            int render_view_routing_id);

  // Returns the RenderFrameProxy for the given routing ID.
  static RenderFrameProxy* FromRoutingID(int routing_id);

  // Returns the RenderFrameProxy given a WebFrame.
  static RenderFrameProxy* FromWebFrame(blink::WebFrame* web_frame);

  virtual ~RenderFrameProxy();

  // IPC::Sender
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Out-of-process child frames receive a signal from RenderWidgetCompositor
  // when a compositor frame has committed.
  void DidCommitCompositorFrame();

  int routing_id() const { return routing_id_; }
  RenderViewImpl* render_view() const { return render_view_; }
  blink::WebFrame* web_frame() const { return web_frame_; }

 private:
  RenderFrameProxy(int routing_id, int frame_routing_id);

  void Init(blink::WebFrame* frame, RenderViewImpl* render_view);

  // IPC::Listener
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // IPC handlers
  void OnDeleteProxy();
  void OnChildFrameProcessGone();
  void OnBuffersSwapped(const FrameMsg_BuffersSwapped_Params& params);
  void OnCompositorFrameSwapped(const IPC::Message& message);

  // The routing ID by which this RenderFrameProxy is known.
  const int routing_id_;

  // The routing ID of the local RenderFrame (if any) which this
  // RenderFrameProxy is meant to replace in the frame tree.
  const int frame_routing_id_;

  // Stores the WebFrame we are associated with.
  // TODO(nick): Make this always a WebRemoteFrame.
  blink::WebFrame* web_frame_;
  scoped_refptr<ChildFrameCompositingHelper> compositing_helper_;

  RenderViewImpl* render_view_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameProxy);
};

}  // namespace

#endif  // CONTENT_RENDERER_RENDER_FRAME_PROXY_H_
