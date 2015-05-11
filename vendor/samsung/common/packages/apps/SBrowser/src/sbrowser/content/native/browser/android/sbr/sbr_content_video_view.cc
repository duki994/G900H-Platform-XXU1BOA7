// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sbrowser/content/native/browser/android/sbr/sbr_content_video_view.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/power_save_blocker_impl.h"
#include "content/common/android/surface_texture_peer.h"
#include "content/public/common/content_switches.h"
#include "out_jni/SbrContentVideoView_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaGlobalRef;

namespace content {

namespace {
// There can only be one content video view at a time, this holds onto that
// singleton instance.
SbrContentVideoView* g_content_video_view = NULL;

}  // namespace

static jobject GetSingletonJavaSbrContentVideoView(JNIEnv*env, jclass) {
  if (g_content_video_view)
    return g_content_video_view->GetJavaObject(env).Release();
  else
    return NULL;
}

bool SbrContentVideoView::RegisterSbrContentVideoView(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SbrContentVideoView* SbrContentVideoView::GetInstance() {
  return g_content_video_view;
}

SbrContentVideoView::SbrContentVideoView(
    BrowserMediaPlayerManager* manager)
    : manager_(manager),
      weak_factory_(this) {
  DCHECK(!g_content_video_view);
  j_content_video_view_ = CreateJavaObject();
  g_content_video_view = this;
}

SbrContentVideoView::~SbrContentVideoView() {
  DCHECK(g_content_video_view);
  DestroyContentVideoView(true);
  g_content_video_view = NULL;
}

void SbrContentVideoView::OpenVideo() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    CreatePowerSaveBlocker();
    Java_SbrContentVideoView_openVideo(env, content_video_view.obj());
  }
}

void SbrContentVideoView::OnMediaPlayerError(int error_type) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    power_save_blocker_.reset();
    Java_SbrContentVideoView_onMediaPlayerError(env, content_video_view.obj(),
        error_type);
  }
}

void SbrContentVideoView::OnVideoSizeChanged(int width, int height) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_SbrContentVideoView_onVideoSizeChanged(env, content_video_view.obj(),
        width, height);
  }
}

void SbrContentVideoView::OnBufferingUpdate(int percent) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_SbrContentVideoView_onBufferingUpdate(env, content_video_view.obj(),
        percent);
  }
}

void SbrContentVideoView::OnPlaybackComplete() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    power_save_blocker_.reset();
    Java_SbrContentVideoView_onPlaybackComplete(env, content_video_view.obj());
  }
}

#if defined(S_MEDIAPLAYER_CONTENTVIDEOVIEW_ONSTART)
void SbrContentVideoView::OnStart() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_SbrContentVideoView_onStart(env, content_video_view.obj());
  }
}
#endif

void SbrContentVideoView::OnExitFullscreen() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_SbrContentVideoView_onExitFullscreen(env, content_video_view.obj());
  }
}

#if defined(S_MEDIAPLAYER_CONTENTVIDEOVIEW_ONMEDIAINTERRUPTED)
void SbrContentVideoView::OnMediaInterrupted() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_SbrContentVideoView_onMediaInterrupted(env, content_video_view.obj());
  }
}
#endif

void SbrContentVideoView::UpdateMediaMetadata() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (content_video_view.is_null())
    return;

  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  if (player && player->IsPlayerReady()) {
    Java_SbrContentVideoView_onUpdateMediaMetadata(
        env, content_video_view.obj(), player->GetVideoWidth(),
        player->GetVideoHeight(), player->GetDuration().InMilliseconds(),
        player->CanPause(),player->CanSeekForward(), player->CanSeekBackward());
  }
}

int SbrContentVideoView::GetVideoWidth(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetVideoWidth() : 0;
}

int SbrContentVideoView::GetVideoHeight(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetVideoHeight() : 0;
}

int SbrContentVideoView::GetDurationInMilliSeconds(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetDuration().InMilliseconds() : -1;
}

int SbrContentVideoView::GetCurrentPosition(JNIEnv*, jobject obj) const {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->GetCurrentTime().InMilliseconds() : 0;
}

bool SbrContentVideoView::IsPlaying(JNIEnv*, jobject obj) {
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  return player ? player->IsPlaying() : false;
}

void SbrContentVideoView::SeekTo(JNIEnv*, jobject obj, jint msec) {
  manager_->FullscreenPlayerSeek(msec);
}

void SbrContentVideoView::Play(JNIEnv*, jobject obj) {
  CreatePowerSaveBlocker();
  manager_->FullscreenPlayerPlay();
}

void SbrContentVideoView::Pause(JNIEnv*, jobject obj) {
  power_save_blocker_.reset();
  manager_->FullscreenPlayerPause();
}

void SbrContentVideoView::ExitFullscreen(
    JNIEnv*, jobject, jboolean release_media_player) {
  power_save_blocker_.reset();
  j_content_video_view_.reset();
  manager_->ExitFullscreen(release_media_player);
}

void SbrContentVideoView::SetSurface(JNIEnv* env, jobject obj,
                                  jobject surface) {
  manager_->SetVideoSurface(
      gfx::ScopedJavaSurface::AcquireExternalSurface(surface));
}

void SbrContentVideoView::RequestMediaMetadata(JNIEnv* env, jobject obj) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SbrContentVideoView::UpdateMediaMetadata,
                 weak_factory_.GetWeakPtr()));
}

ScopedJavaLocalRef<jobject> SbrContentVideoView::GetJavaObject(JNIEnv* env) {
  return j_content_video_view_.get(env);
}

gfx::NativeView SbrContentVideoView::GetNativeView() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (content_video_view.is_null())
    return NULL;

  return reinterpret_cast<gfx::NativeView>(
      Java_SbrContentVideoView_getNativeViewAndroid(env,
                                                 content_video_view.obj()));

}

JavaObjectWeakGlobalRef SbrContentVideoView::CreateJavaObject() {
  ContentViewCoreImpl* content_view_core = manager_->GetContentViewCore();
  JNIEnv* env = AttachCurrentThread();
  int height = 0, width = 0;
  media::MediaPlayerAndroid* player = manager_->GetFullscreenPlayer();
  if (player && player->IsPlayerReady()) {
    width = player->GetVideoWidth();
    height = player->GetVideoHeight();
  }
  bool legacyMode = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableOverlayFullscreenVideoSubtitle);
  return JavaObjectWeakGlobalRef(
      env,
      Java_SbrContentVideoView_createSbrContentVideoView(
          env,
          content_view_core->GetContext().obj(),
          reinterpret_cast<intptr_t>(this),
          content_view_core->GetContentVideoViewClient().obj(),
          legacyMode, width, height).obj());
}

void SbrContentVideoView::CreatePowerSaveBlocker() {
  if (power_save_blocker_) return;

  power_save_blocker_ = PowerSaveBlocker::Create(
      PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep,
      "Playing video").Pass();
  static_cast<PowerSaveBlockerImpl*>(power_save_blocker_.get())->
      InitDisplaySleepBlocker(GetNativeView());
}

void SbrContentVideoView::DestroyContentVideoView(bool native_view_destroyed) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
  if (!content_video_view.is_null()) {
    Java_SbrContentVideoView_destroyContentVideoView(env,
        content_video_view.obj(), native_view_destroyed);
    j_content_video_view_.reset();
  }
}

#if defined(S_MEDIAPLAYER_FULLSCREEN_CLOSEDCAPTION_SUPPORT)
void SbrContentVideoView::UpdateCCVisibility(int status){
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> content_video_view = GetJavaObject(env);
    if (!content_video_view.is_null()) {
        Java_SbrContentVideoView_onUpdateCCVisibility(env, content_video_view.obj(),
            status);
      }
}
void SbrContentVideoView::SetCCVisibility(JNIEnv*, jobject obj, bool visible) {
    manager_->SetFullscreenCCVisibility(visible);
}
#endif

}  // namespace content
