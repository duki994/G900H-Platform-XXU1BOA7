// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_player_listener.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_source_player.h"

// Auto generated jni class from MediaPlayerListener.java.
// Check base/android/jni_generator/golden_sample_for_tests_jni.h for example.
#include "jni/MediaPlayerListener_jni.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ScopedJavaLocalRef;

namespace media {

MediaPlayerListener::MediaPlayerListener(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<MediaPlayerBridge> media_player)
    : task_runner_(task_runner),
      media_player_(media_player) {
  DCHECK(task_runner_.get());
  DCHECK(media_player_);
}

MediaPlayerListener::MediaPlayerListener(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    base::WeakPtr<MediaSourcePlayer> media_source_player)
    : task_runner_(task_runner),
      media_source_player_(media_source_player) {
  DCHECK(task_runner_.get());
  DCHECK(media_source_player_);
}

MediaPlayerListener::~MediaPlayerListener() {}

void MediaPlayerListener::CreateMediaPlayerListener(
    jobject context, jobject media_player_bridge) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  j_media_player_listener_.Reset(
      Java_MediaPlayerListener_create(
          env, reinterpret_cast<intptr_t>(this), context, media_player_bridge));
}

void MediaPlayerListener::CreateMediaPlayerListener(jobject context) {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  j_media_player_listener_.Reset(
      Java_MediaPlayerListener_createSourceListener(
          env, reinterpret_cast<intptr_t>(this), context));
}

void MediaPlayerListener::ReleaseMediaPlayerListenerResources() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (!j_media_player_listener_.is_null()) {
    Java_MediaPlayerListener_releaseResources(
        env, j_media_player_listener_.obj());
  }
  j_media_player_listener_.Reset();
}

bool MediaPlayerListener::RequestAudioFocus() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (!j_media_player_listener_.is_null()) {
    return Java_MediaPlayerListener_requestAudioFocus(env,
                                                j_media_player_listener_.obj());
  }
  return false;
}

void MediaPlayerListener::ShowAudioFocusRequestFailMessage() {
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (!j_media_player_listener_.is_null()) {
    Java_MediaPlayerListener_showAudioFocusRequestFailMessage(env,
                                                j_media_player_listener_.obj());
  }
}

void MediaPlayerListener::OnMediaError(
    JNIEnv* /* env */, jobject /* obj */, jint error_type) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnMediaError, media_player_, error_type));
}

void MediaPlayerListener::OnVideoSizeChanged(
    JNIEnv* /* env */, jobject /* obj */, jint width, jint height) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnVideoSizeChanged, media_player_,
      width, height));
}

void MediaPlayerListener::OnBufferingUpdate(
    JNIEnv* /* env */, jobject /* obj */, jint percent) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnBufferingUpdate, media_player_, percent));
}

void MediaPlayerListener::OnPlaybackComplete(
    JNIEnv* /* env */, jobject /* obj */) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnPlaybackComplete, media_player_));
}

void MediaPlayerListener::OnSeekComplete(
    JNIEnv* /* env */, jobject /* obj */) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnSeekComplete, media_player_));
}

void MediaPlayerListener::OnMediaPrepared(
    JNIEnv* /* env */, jobject /* obj */) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnMediaPrepared, media_player_));
}

void MediaPlayerListener::OnMediaInterrupted(
    JNIEnv* /* env */, jobject /* obj */) {
  if (media_player_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnMediaInterrupted, media_player_));
  }
  if (media_source_player_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaSourcePlayer::OnMediaInterrupted, media_source_player_));
  }
}


void MediaPlayerListener::OnAudioFocusGain(
  JNIEnv* /* env */, jobject /* obj */) {
#if defined(S_MEDIAPLAYER_AUDIOFOCUS_GAIN_EVENT_FIX)
  if (media_player_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaPlayerBridge::OnAudioFocusGain, media_player_));
  }
  if (media_source_player_) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
      &MediaSourcePlayer::OnAudioFocusGain, media_source_player_));
  }
#endif
}


bool MediaPlayerListener::RegisterMediaPlayerListener(JNIEnv* env) {
  bool ret = RegisterNativesImpl(env);
  DCHECK(g_MediaPlayerListener_clazz);
  return ret;
}

void MediaPlayerListener::AbandonAudioFocus(){
  JNIEnv* env = AttachCurrentThread();
  CHECK(env);
  if (!j_media_player_listener_.is_null()) {
    Java_MediaPlayerListener_abandonAudioFocus(
        env, j_media_player_listener_.obj());
  }
}

}  // namespace media
