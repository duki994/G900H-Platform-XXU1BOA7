// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_glow.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/threading/worker_pool.h"
#include "cc/layers/image_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "content/browser/android/edge_effect_l.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/screen.h"

using std::max;
using std::min;

namespace content {

namespace {

const float kEpsilon = 1e-3f;

class OverscrollResources {
 public:
  OverscrollResources() {
    TRACE_EVENT0("browser", "OverscrollResources::Create");
    edge_bitmap_ =
        gfx::CreateSkBitmapFromResource("android:drawable/overscroll_edge",
                                        gfx::Size(128, 12));
    glow_bitmap_ =
        gfx::CreateSkBitmapFromResource("android:drawable/overscroll_glow",
                                        gfx::Size(128, 64));
  }

  const SkBitmap& edge_bitmap() { return edge_bitmap_; }
  const SkBitmap& glow_bitmap() { return glow_bitmap_; }

 private:
  SkBitmap edge_bitmap_;
  SkBitmap glow_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollResources);
};

class OverscrollLResources {
 public:
  OverscrollLResources() {
    TRACE_EVENT0("browser", "OverscrollLResources::Create");
    glow_l_bitmap_ = CreateOverGlowLBitmap();
  }

  const SkBitmap& glow_l_bitmap() { return glow_l_bitmap_; }

 private:
  SkBitmap glow_l_bitmap_;
  SkBitmap CreateOverGlowLBitmap() {
    const float kSin = 0.5f;    // sin(PI / 6);
    const float kCos = 0.866f;  // cos(PI / 6);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setAlpha(0xBB);
    paint.setStyle(SkPaint::kFill_Style);

    gfx::Size screen_size =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().GetSizeInPixel();
    const float arc_width =
      std::min(screen_size.width(), screen_size.height()) * 0.5f / kSin;
    const float y = kCos * arc_width;
    const float height = arc_width - y;
    gfx::Size bounds(arc_width, height);
    SkRect arc_rect = SkRect::MakeXYWH(
        -arc_width / 2.f, -arc_width - y, arc_width * 2.f, arc_width * 2.f);
    SkBitmap glow_bitmap;
    if (!glow_bitmap.allocPixels(
          SkImageInfo::MakeA8(bounds.width(), bounds.height()))) {
      LOG(FATAL) << "Failed to allocate bitmap of size " << bounds.width() << "X"
                 << bounds.height();
    }

    glow_bitmap.eraseColor(SK_ColorTRANSPARENT);

    SkCanvas canvas(glow_bitmap);
    canvas.clipRect(SkRect::MakeXYWH(0, 0, bounds.width(), bounds.height()));
    canvas.drawArc(arc_rect, 45, 90, true, paint);
    return glow_bitmap;
  }

  DISALLOW_COPY_AND_ASSIGN(OverscrollLResources);
};

base::LazyInstance<OverscrollLResources>::Leaky g_overscroll_l_resources =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<cc::UIResourceLayer> CreateImageLayer(const SkBitmap& bitmap) {
  scoped_refptr<cc::UIResourceLayer> layer = cc::UIResourceLayer::Create();
  layer->SetBitmap(bitmap);
  return layer;
}

bool IsApproxZero(float value) {
  return std::abs(value) < kEpsilon;
}

gfx::Vector2dF ZeroSmallComponents(gfx::Vector2dF vector) {
  if (IsApproxZero(vector.x()))
    vector.set_x(0);
  if (IsApproxZero(vector.y()))
    vector.set_y(0);
  return vector;
}

gfx::Transform ComputeTransform(OverscrollGlow::Edge edge,
                                const gfx::SizeF& window_size,
                                float offset) {
  switch (edge) {
    case OverscrollGlow::EDGE_TOP:
      return gfx::Transform(1, 0, 0, 1, 0, offset);
    case OverscrollGlow::EDGE_LEFT:
      return gfx::Transform(0,
                            1,
                            -1,
                            0,
                            offset,
                            window_size.height());
    case OverscrollGlow::EDGE_BOTTOM:
      return gfx::Transform(-1, 0, 0, -1, window_size.width(), window_size.height() + offset);
    case OverscrollGlow::EDGE_RIGHT:
      return gfx::Transform(
          0,
          -1,
          1,
          0,
          window_size.width() + offset,
          0);
    default:
      NOTREACHED() << "Invalid edge: " << edge;
      return gfx::Transform();
  };
}

gfx::SizeF ComputeSize(OverscrollGlow::Edge edge,
                       const gfx::SizeF& window_size) {
  switch (edge) {
    case OverscrollGlow::EDGE_TOP:
    case OverscrollGlow::EDGE_BOTTOM:
      return window_size;
    case OverscrollGlow::EDGE_LEFT:
    case OverscrollGlow::EDGE_RIGHT:
      return gfx::SizeF(window_size.height(), window_size.width());
    default:
      NOTREACHED() << "Invalid edge: " << edge;
      return gfx::SizeF();
  };
}

// Force loading of any necessary resources.  This function is thread-safe.
void EnsureResources() {
    g_overscroll_l_resources.Get();
}

} // namespace

scoped_ptr<OverscrollGlow> OverscrollGlow::Create(bool enabled) {
  // Don't block the main thread with effect resource loading during creation.
  // Effect instantiation is deferred until the effect overscrolls, in which
  // case the main thread may block until the resource has loaded.
  if (enabled && g_overscroll_l_resources == NULL) {
    base::WorkerPool::PostTask(FROM_HERE, base::Bind(EnsureResources), true);
  }

  return make_scoped_ptr(new OverscrollGlow(enabled));
}

OverscrollGlow::OverscrollGlow(bool enabled)
  : enabled_(enabled),
    initialized_(false) {}

OverscrollGlow::~OverscrollGlow() {
  Detach();
}

void OverscrollGlow::Enable() {
  enabled_ = true;
}

void OverscrollGlow::Disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  if (!enabled_ && initialized_) {
    Detach();
    for (size_t i = 0; i < EDGE_COUNT; ++i)
      edge_effects_[i]->Finish();
  }
}

bool OverscrollGlow::OnOverscrolled(cc::Layer* overscrolling_layer,
                                    base::TimeTicks current_time,
                                    gfx::Vector2dF accumulated_overscroll,
                                    gfx::Vector2dF overscroll_delta,
                                    gfx::Vector2dF velocity,
                                    gfx::Vector2dF displacement) {
  DCHECK(overscrolling_layer);

  if (!enabled_)
    return false;

  // The size of the glow determines the relative effect of the inputs; an
  // empty-sized effect is effectively disabled.
  if (display_params_.size.IsEmpty())
    return false;

  // Ignore sufficiently small values that won't meaningfuly affect animation.
  overscroll_delta = ZeroSmallComponents(overscroll_delta);

  if (overscroll_delta.IsZero()) {
    if (initialized_) {
      Release(current_time);
      UpdateLayerAttachment(overscrolling_layer);
    }
    return NeedsAnimate();
  }

  if (!InitializeIfNecessary())
    return false;

  gfx::Vector2dF old_overscroll = accumulated_overscroll - overscroll_delta;
  bool x_overscroll_started =
      !IsApproxZero(overscroll_delta.x()) && IsApproxZero(old_overscroll.x());
  bool y_overscroll_started =
      !IsApproxZero(overscroll_delta.y()) && IsApproxZero(old_overscroll.y());

  velocity = ZeroSmallComponents(velocity);
  if (!velocity.IsZero())
    Absorb(current_time, velocity, x_overscroll_started, y_overscroll_started);
  else
    Pull(current_time, overscroll_delta, displacement);

  UpdateLayerAttachment(overscrolling_layer);
  return NeedsAnimate();
}

bool OverscrollGlow::Animate(base::TimeTicks current_time) {
  if (!NeedsAnimate()) {
    Detach();
    return false;
  }

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (edge_effects_[i]->Update(current_time)) {
      Edge edge = static_cast<Edge>(i);
      edge_effects_[i]->ApplyToLayers(
          ComputeSize(edge, display_params_.size),
          ComputeTransform(
              edge, display_params_.size, display_params_.edge_offsets[i]));
    }
  }

  if (!NeedsAnimate()) {
    Detach();
    return false;
  }

  return true;
}


void OverscrollGlow::UpdateDisplayParameters(const DisplayParameters& params) {
  display_params_ = params;
}

bool OverscrollGlow::NeedsAnimate() const {
  if (!enabled_ || !initialized_)
    return false;
  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!edge_effects_[i]->IsFinished())
      return true;
  }
  return false;
}

void OverscrollGlow::UpdateLayerAttachment(cc::Layer* parent) {
  DCHECK(parent);
  if (!root_layer_)
    return;

  if (!NeedsAnimate()) {
    Detach();
    return;
  }

  if (root_layer_->parent() != parent)
    parent->AddChild(root_layer_);
}

void OverscrollGlow::Detach() {
  if (root_layer_)
    root_layer_->RemoveFromParent();
}

bool OverscrollGlow::InitializeIfNecessary() {
  DCHECK(enabled_);
  if (initialized_)
    return true;

    const SkBitmap& glow_l = g_overscroll_l_resources.Get().glow_l_bitmap();
    if (glow_l.isNull()) {
      Disable();
      return false;
    }

  DCHECK(!root_layer_);
  root_layer_ = cc::Layer::Create();
  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    scoped_refptr<cc::UIResourceLayer> glow_l_layer = CreateImageLayer(glow_l);
    root_layer_->AddChild(glow_l_layer);
    edge_effects_[i] = make_scoped_ptr(new EdgeEffectL(glow_l_layer));
  }

  initialized_ = true;
  return true;
}

void OverscrollGlow::Pull(base::TimeTicks current_time,
                          gfx::Vector2dF overscroll_delta,
                          gfx::Vector2dF overscroll_location) {
  DCHECK(enabled_ && initialized_);
  if (overscroll_delta.IsZero())
    return;

  const float inv_width = 1.f / display_params_.size.width();
  const float inv_height = 1.f / display_params_.size.height();

  gfx::Vector2dF overscroll_pull =
      gfx::ScaleVector2d(overscroll_delta, inv_width, inv_height);
  const float edge_overscroll_pull[EDGE_COUNT] = {
    min(overscroll_pull.y(), 0.f), // Top
    min(overscroll_pull.x(), 0.f), // Left
    max(overscroll_pull.y(), 0.f), // Bottom
    max(overscroll_pull.x(), 0.f)  // Right
  };

  gfx::Vector2dF displacement =
      gfx::ScaleVector2d(overscroll_location, inv_width, inv_height);
  displacement.set_x(max(0.f, min(1.f, displacement.x())));
  displacement.set_y(max(0.f, min(1.f, displacement.y())));

  const float edge_displacement[EDGE_COUNT] = {
    1.f - displacement.x(), // Top
    displacement.y(),       // Left
    displacement.x(),       // Bottom
    1.f - displacement.y()  // Right
  };

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!edge_overscroll_pull[i])
      continue;

    edge_effects_[i]->Pull(
        current_time, std::abs(edge_overscroll_pull[i]), edge_displacement[i]);
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Absorb(base::TimeTicks current_time,
                            gfx::Vector2dF velocity,
                            bool x_overscroll_started,
                            bool y_overscroll_started) {
  DCHECK(enabled_ && initialized_);
  if (velocity.IsZero())
    return;

  // Only trigger on initial overscroll at a non-zero velocity
  const float overscroll_velocities[EDGE_COUNT] = {
    y_overscroll_started ? min(velocity.y(), 0.f) : 0,  // Top
    x_overscroll_started ? min(velocity.x(), 0.f) : 0,  // Left
    y_overscroll_started ? max(velocity.y(), 0.f) : 0,  // Bottom
    x_overscroll_started ? max(velocity.x(), 0.f) : 0   // Right
  };

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    if (!overscroll_velocities[i])
      continue;

    edge_effects_[i]->Absorb(current_time, std::abs(overscroll_velocities[i]));
    GetOppositeEdge(i)->Release(current_time);
  }
}

void OverscrollGlow::Release(base::TimeTicks current_time) {
  DCHECK(initialized_);

  for (size_t i = 0; i < EDGE_COUNT; ++i) {
    edge_effects_[i]->Release(current_time);
  }
}

EdgeEffectL* OverscrollGlow::GetOppositeEdge(int edge_index) {
  DCHECK(initialized_);
  return edge_effects_[(edge_index + 2) % EDGE_COUNT].get();
}

OverscrollGlow::DisplayParameters::DisplayParameters()
    : device_scale_factor(1) {
  edge_offsets[0] = edge_offsets[1] = edge_offsets[2] = edge_offsets[3] = 0.f;
}

}  // namespace content

