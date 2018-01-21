// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/shared_renderer_state.h"

#include "android_webview/browser/browser_view_renderer_client.h"
#include "base/bind.h"
#include "base/location.h"

namespace android_webview {

DrawGLInput::DrawGLInput() : width(0), height(0) {
}

DrawGLInput::~DrawGLInput() {
}

SharedRendererState::SharedRendererState(
    scoped_refptr<base::MessageLoopProxy> ui_loop,
    BrowserViewRendererClient* client)
    : ui_loop_(ui_loop),
      client_on_ui_(client),
      weak_factory_on_ui_thread_(this),
      ui_thread_weak_ptr_(weak_factory_on_ui_thread_.GetWeakPtr()),
      inside_hardware_release_(false),
      share_context_(NULL) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(client_on_ui_);
}

SharedRendererState::~SharedRendererState() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
}

void SharedRendererState::ClientRequestDrawGL() {
  if (ui_loop_->BelongsToCurrentThread()) {
    ClientRequestDrawGLOnUIThread();
  } else {
    ui_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SharedRendererState::ClientRequestDrawGLOnUIThread,
                   ui_thread_weak_ptr_));
  }
}

void SharedRendererState::ClientRequestDrawGLOnUIThread() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  if (!client_on_ui_->RequestDrawGL(NULL, false)) {
    LOG(ERROR) << "Failed to request GL process. Deadlock likely";
  }
}

void SharedRendererState::SetDrawGLInput(scoped_ptr<DrawGLInput> input) {
  base::AutoLock lock(lock_);
  DCHECK(!draw_gl_input_.get());
  draw_gl_input_ = input.Pass();
}

scoped_ptr<DrawGLInput> SharedRendererState::PassDrawGLInput() {
  base::AutoLock lock(lock_);
  return draw_gl_input_.Pass();
}

void SharedRendererState::SetInsideHardwareRelease(bool inside) {
  base::AutoLock lock(lock_);
  inside_hardware_release_ = inside;
}

bool SharedRendererState::IsInsideHardwareRelease() const {
  base::AutoLock lock(lock_);
  return inside_hardware_release_;
}

void SharedRendererState::SetSharedContext(gpu::GLInProcessContext* context) {
  base::AutoLock lock(lock_);
  DCHECK(!share_context_ || !context);
  share_context_ = context;
}

gpu::GLInProcessContext* SharedRendererState::GetSharedContext() const {
  base::AutoLock lock(lock_);
  DCHECK(share_context_);
  return share_context_;
}

void SharedRendererState::InsertReturnedResources(
    const cc::ReturnedResourceArray& resources) {
  base::AutoLock lock(lock_);
  returned_resources_.insert(
      returned_resources_.end(), resources.begin(), resources.end());
}

void SharedRendererState::SwapReturnedResources(
    cc::ReturnedResourceArray* resources) {
  DCHECK(resources->empty());
  base::AutoLock lock(lock_);
  resources->swap(returned_resources_);
}

bool SharedRendererState::ReturnedResourcesEmpty() const {
  base::AutoLock lock(lock_);
  return returned_resources_.empty();
}

InsideHardwareReleaseReset::InsideHardwareReleaseReset(
    SharedRendererState* shared_renderer_state)
    : shared_renderer_state_(shared_renderer_state) {
  DCHECK(!shared_renderer_state_->IsInsideHardwareRelease());
  shared_renderer_state_->SetInsideHardwareRelease(true);
}

InsideHardwareReleaseReset::~InsideHardwareReleaseReset() {
  shared_renderer_state_->SetInsideHardwareRelease(false);
}

}  // namespace android_webview
