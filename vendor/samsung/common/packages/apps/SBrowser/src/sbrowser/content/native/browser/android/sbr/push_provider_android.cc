// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_PUSH_API)

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/push_registration/push_registration_message_filter.h"
#include "sbrowser/content/native/browser/android/sbr/push_provider_android.h"

#include "out_jni/PushProvider_jni.h"

#include "url/gurl.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::GetApplicationContext;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertJavaStringToUTF16;
using base::android::ScopedJavaLocalRef;

namespace content {

enum {
  PUSH_FAIL = 0,
  PUSH_SUCCESS
};

class PushProviderAndroid::CallbackDispatcher {
public:
  static CallbackDispatcher* Create(const RegistrationCallback& callback) {
    CallbackDispatcher* dispatcher = new CallbackDispatcher;
    dispatcher->register_callback_ = callback;
    return dispatcher;
  }

  static CallbackDispatcher* Create(const UnregistrationCallback& callback) {
    CallbackDispatcher* dispatcher = new CallbackDispatcher;
    dispatcher->unregister_callback_ = callback;
    return dispatcher;
  }

  ~CallbackDispatcher() {}

  void DidRegister(
      base::string16& endpoint,
      base::string16& registrationId,
      bool error) {
    register_callback_.Run(endpoint, registrationId, error);
  }

  void DidUnregister(bool result) {
    unregister_callback_.Run(result);
  }

private:
  CallbackDispatcher() {}

  RegistrationCallback register_callback_;
  UnregistrationCallback unregister_callback_;

  DISALLOW_COPY_AND_ASSIGN(CallbackDispatcher);

};

PushProviderAndroid::PushProviderAndroid() {
}

PushProviderAndroid::~PushProviderAndroid() {
  for (IDMap<CallbackDispatcher, IDMapOwnPointer>::iterator iter(&push_dispatcher_);
        !iter.IsAtEnd(); iter.Advance()) {
    int request_id = iter.GetCurrentKey();
    CallbackDispatcher* dispatcher = iter.GetCurrentValue();
    DCHECK(dispatcher);
    push_dispatcher_.Remove(request_id);
  }
}

bool PushProviderAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void PushProviderAndroid::Register(
    const GURL& origin,
    const RegistrationCallback& callback) {
  JNIEnv* env = AttachCurrentThread();

  if (j_push_provider_.is_null()) {
    j_push_provider_.Reset(Java_PushProvider_create(
        env,
        reinterpret_cast<intptr_t>(this)));
  }

  int request_id = push_dispatcher_.Add(
      CallbackDispatcher::Create(callback));

  Java_PushProvider_register(
      env,
      j_push_provider_.obj(),
      GetApplicationContext(),
      ConvertUTF8ToJavaString(env, origin.spec()).obj(),
      request_id);
}

void PushProviderAndroid::Unregister(
    const GURL& origin,
    const UnregistrationCallback& callback) {
  JNIEnv* env = AttachCurrentThread();

  if (j_push_provider_.is_null()) {
    j_push_provider_.Reset(Java_PushProvider_create(
        env,
        reinterpret_cast<intptr_t>(this)));
  }

  int request_id = push_dispatcher_.Add(
      CallbackDispatcher::Create(callback));

  Java_PushProvider_unregister(
      env,
      j_push_provider_.obj(),
      GetApplicationContext(),
      ConvertUTF8ToJavaString(env, origin.spec()).obj(),
      request_id);
}

void PushProviderAndroid::IsRegistered(
    const GURL& origin,
    const IsRegisteredCallback& callback) {
  JNIEnv* env = AttachCurrentThread();

  if (j_push_provider_.is_null()) {
    j_push_provider_.Reset(Java_PushProvider_create(
        env,
        reinterpret_cast<intptr_t>(this)));
  }

  jboolean j_is_registered = Java_PushProvider_isRegistered(
      env,
      j_push_provider_.obj(),
      GetApplicationContext(),
      ConvertUTF8ToJavaString(env,origin.spec()).obj());

  callback.Run((bool)j_is_registered, false);
}

void PushProviderAndroid::DidRegister(
    JNIEnv* env,
    jobject obj,
    jint result,
    jobject j_push_registration,
    jint request_id) {
  CallbackDispatcher* dispatcher = (push_dispatcher_.Lookup(
      static_cast<int>(request_id)));
  DCHECK(dispatcher);

  base::string16 endpoint;
  base::string16 registrationId;

  if(result == PUSH_FAIL) {
    LOG(ERROR) << "Push Register is Fail";
    dispatcher->DidRegister(endpoint, registrationId, true);
    push_dispatcher_.Remove(static_cast<int>(request_id));
    return;
  }

  jclass j_push_registraion_class = env->GetObjectClass(j_push_registration);

  jfieldID j_end_point_field = env->GetFieldID(
      j_push_registraion_class,
      "endPoint",
      "Ljava/lang/String;");
  jfieldID j_registration_id_field = env->GetFieldID(
      j_push_registraion_class,
      "pushID","Ljava/lang/String;");

  endpoint = ConvertJavaStringToUTF16(env, (jstring)env->GetObjectField(
      j_push_registration, j_end_point_field));
  registrationId = ConvertJavaStringToUTF16(env, (jstring)env->GetObjectField(
      j_push_registration, j_registration_id_field));

  dispatcher->DidRegister(endpoint, registrationId, false);
  push_dispatcher_.Remove(static_cast<int>(request_id));
}

void PushProviderAndroid::DidUnregister(
    JNIEnv* env,
    jobject obj,
    jint result,
    jint request_id) {
  CallbackDispatcher* dispatcher = push_dispatcher_.Lookup(
      static_cast<int>(request_id));
  DCHECK(dispatcher);

  dispatcher->DidUnregister(
      (static_cast<int>(result) == PUSH_FAIL ? true : false));

  push_dispatcher_.Remove(static_cast<int>(request_id));
}

PushProvider* PushRegistrationMessageFilter::CreateProvider() {
  return (new PushProviderAndroid());
}
} // namespace content

#endif // defined(ENABLE_PUSH_API)
