// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_surface.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {

namespace {

scoped_refptr<DriBuffer> AllocateBuffer(DriWrapper* dri,
                                        const gfx::Size& size) {
  scoped_refptr<DriBuffer> buffer(new DriBuffer(dri));
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  CHECK(buffer->Initialize(info)) << "Failed to create drm buffer.";

  return buffer;
}

}  // namespace

DriSurface::DriSurface(
    DriWrapper* dri,
    const base::WeakPtr<HardwareDisplayController>& controller)
    : dri_(dri),
      buffers_(),
      front_buffer_(0),
      controller_(controller) {
}

DriSurface::~DriSurface() {
}

skia::RefPtr<SkCanvas> DriSurface::GetCanvas() {
  return skia::SharePtr(surface_->getCanvas());
}

void DriSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  SkImageInfo info = SkImageInfo::MakeN32(
      viewport_size.width(), viewport_size.height(), kOpaque_SkAlphaType);
  surface_ = skia::AdoptRef(SkSurface::NewRaster(info));

  if (!controller_)
    return;

  // For the display buffers use the mode size since a |viewport_size| smaller
  // than the display size will not scanout.
  gfx::Size mode_size(controller_->get_mode().hdisplay,
                      controller_->get_mode().vdisplay);
  for (size_t i = 0; i < arraysize(buffers_); ++i)
    buffers_[i] = AllocateBuffer(dri_, mode_size);
}

void DriSurface::PresentCanvas(const gfx::Rect& damage) {
  CHECK(base::MessageLoopForUI::IsCurrent());
  CHECK(buffers_[front_buffer_ ^ 1]);

  if (!controller_)
    return;

  std::vector<OverlayPlane> planes(
      1, OverlayPlane(buffers_[front_buffer_ ^ 1]));

  UpdateNativeSurface(damage);
  if (controller_->SchedulePageFlip(planes))
    controller_->WaitForPageFlipEvent();

  // Update our front buffer pointer.
  front_buffer_ ^= 1;
}

scoped_ptr<gfx::VSyncProvider> DriSurface::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>(new DriVSyncProvider(controller_));
}

void DriSurface::UpdateNativeSurface(const gfx::Rect& damage) {
  SkCanvas* canvas = buffers_[front_buffer_ ^ 1]->GetCanvas();

  // The DriSurface is double buffered, so the current back buffer is
  // missing the previous update. Expand damage region.
  SkRect real_damage = RectToSkRect(UnionRects(damage, last_damage_));

  // Copy damage region.
  skia::RefPtr<SkImage> image = skia::AdoptRef(surface_->newImageSnapshot());
  image->draw(canvas, &real_damage, real_damage, NULL);

  last_damage_ = damage;
}

}  // namespace ui
