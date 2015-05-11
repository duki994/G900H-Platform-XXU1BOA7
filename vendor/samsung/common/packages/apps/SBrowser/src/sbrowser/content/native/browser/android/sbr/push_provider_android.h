// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_PUSH_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_PUSH_PROVIDER_ANDROID_H_

#if defined(ENABLE_PUSH_API)

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "content/browser/push_registration/push_provider.h"

class GURL;

namespace content {

class PushProviderAndroid : public PushProvider {
 public:
  PushProviderAndroid();
  static bool Register(JNIEnv* env);

  virtual void Register(
      const GURL& origin,
      const RegistrationCallback& callback) OVERRIDE;
  virtual void Unregister(
      const GURL& origin,
      const UnregistrationCallback& callback) OVERRIDE;
  virtual void IsRegistered(
      const GURL& origin,
      const IsRegisteredCallback& callback) OVERRIDE;

//  virtual PushProvider* CreateProvider() OVERRIDE;

  // Call by Java (Aync API)
  void DidRegister(
      JNIEnv* env,
      jobject obj,
      jint result,
      jobject j_push_registration,
      jint request_id);

  void DidUnregister(
      JNIEnv* env,
      jobject obj,
      jint result,
      jint request_id);

 private:
  virtual ~PushProviderAndroid();

  class CallbackDispatcher;

  base::android::ScopedJavaGlobalRef<jobject> j_push_provider_;
  IDMap<CallbackDispatcher, IDMapOwnPointer> push_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PushProviderAndroid);
};

} // namespace content

#endif // defined(ENABLE_PUSH_API)

#endif // CONTENT_BROWSER_ANDROID_PUSH_PROVIDER_ANDROID_H_
