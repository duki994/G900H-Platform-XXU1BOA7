// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_EDGE_EFFECT_L_H_
#define CONTENT_BROWSER_ANDROID_EDGE_EFFECT_L_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace cc {
class Layer;
class UIResourceLayer;
}

namespace content {

// |EdgeEffectL| mirrors its Android L counterpart, EdgeEffect.java.
// Conscious tradeoffs were made to align this as closely as possible with the
// the original Android java version.
// All coordinates and dimensions are in device pixels.
class EdgeEffectL {
 public:
  enum State {
    STATE_IDLE = 0,
    STATE_PULL,
    STATE_ABSORB,
    STATE_RECEDE,
    STATE_PULL_DECAY
  };

  explicit EdgeEffectL(scoped_refptr<cc::UIResourceLayer> glow_l);
  ~EdgeEffectL();

  void Pull(base::TimeTicks current_time,
                    float delta_distance,
                    float displacement);
  void Absorb(base::TimeTicks current_time, float velocity);
  bool Update(base::TimeTicks current_time);
  void Release(base::TimeTicks current_time);

  void Finish();
  bool IsFinished() const;

  void ApplyToLayers(const gfx::SizeF& size,
                     const gfx::Transform& transform);

 private:
  scoped_refptr<cc::UIResourceLayer> glow_;

  float glow_alpha_;
  float glow_scale_y_;

  float glow_alpha_start_;
  float glow_alpha_finish_;
  float glow_scale_y_start_;
  float glow_scale_y_finish_;

  gfx::RectF arc_rect_;
  gfx::Size bounds_;
  float displacement_;
  float target_displacement_;

  base::TimeTicks start_time_;
  base::TimeDelta duration_;

  State state_;

  float pull_distance_;

  DISALLOW_COPY_AND_ASSIGN(EdgeEffectL);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_EDGE_EFFECT_L_H_
