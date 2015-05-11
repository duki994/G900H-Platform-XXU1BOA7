// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/chrome_http_auth_handler.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "grit/generated_resources.h"
#include "jni/ChromeHttpAuthHandler_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;
#if defined(S_USE_SYSTEM_PROXY_AUTH_CREDENTIAL)
ChromeHttpAuthHandler::ChromeHttpAuthHandler(const base::string16& explanation)
    : observer_(NULL),
      explanation_(explanation),
      is_proxy_auth_(false),
      did_use_http_auth_(false) {
}

ChromeHttpAuthHandler::ChromeHttpAuthHandler(
    const base::string16& explanation,
    bool is_proxy_auth,
    bool did_use_http_auth)
    : observer_(NULL),
      explanation_(explanation),
      is_proxy_auth_(is_proxy_auth),
      did_use_http_auth_(did_use_http_auth) {
}
#else
ChromeHttpAuthHandler::ChromeHttpAuthHandler(const base::string16& explanation)
    : observer_(NULL),
      explanation_(explanation) {
}
#endif
ChromeHttpAuthHandler::~ChromeHttpAuthHandler() {}

void ChromeHttpAuthHandler::Init() {
  DCHECK(java_chrome_http_auth_handler_.is_null());
  JNIEnv* env = AttachCurrentThread();
  java_chrome_http_auth_handler_.Reset(
      Java_ChromeHttpAuthHandler_create(env, reinterpret_cast<intptr_t>(this)));
}

void ChromeHttpAuthHandler::SetObserver(LoginHandler* observer) {
  observer_ = observer;
}

jobject ChromeHttpAuthHandler::GetJavaObject() {
  return java_chrome_http_auth_handler_.obj();
}

void ChromeHttpAuthHandler::OnAutofillDataAvailable(
    const base::string16& username,
    const base::string16& password) {
  DCHECK(java_chrome_http_auth_handler_.obj() != NULL);
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_username =
      ConvertUTF16ToJavaString(env, username);
  ScopedJavaLocalRef<jstring> j_password =
      ConvertUTF16ToJavaString(env, password);
  Java_ChromeHttpAuthHandler_onAutofillDataAvailable(
      env, java_chrome_http_auth_handler_.obj(),
      j_username.obj(), j_password.obj());
}

void ChromeHttpAuthHandler::SetAuth(JNIEnv* env,
                                    jobject,
                                    jstring username,
                                    jstring password) {
  if (observer_) {
    base::string16 username16 = ConvertJavaStringToUTF16(env, username);
    base::string16 password16 = ConvertJavaStringToUTF16(env, password);
    observer_->SetAuth(username16, password16);
  }
}

void ChromeHttpAuthHandler::CancelAuth(JNIEnv* env, jobject) {
  if (observer_)
    observer_->CancelAuth();
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetMessageTitle(
    JNIEnv* env, jobject) {
  return ConvertUTF16ToJavaString(env,
      l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_TITLE));
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetMessageBody(
    JNIEnv* env, jobject) {
  return ConvertUTF16ToJavaString(env, explanation_);
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetUsernameLabelText(
    JNIEnv* env, jobject) {
  return ConvertUTF16ToJavaString(env,
      l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_USERNAME_FIELD));
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetPasswordLabelText(
    JNIEnv* env, jobject) {
  return ConvertUTF16ToJavaString(env,
      l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_PASSWORD_FIELD));
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetOkButtonText(
    JNIEnv* env, jobject) {
  return ConvertUTF16ToJavaString(env,
      l10n_util::GetStringUTF16(IDS_LOGIN_DIALOG_OK_BUTTON_LABEL));
}

ScopedJavaLocalRef<jstring> ChromeHttpAuthHandler::GetCancelButtonText(
    JNIEnv* env, jobject) {
  return ConvertUTF16ToJavaString(env, l10n_util::GetStringUTF16(IDS_CANCEL));
}
// #if defined(S_USE_SYSTEM_PROXY_AUTH_CREDENTIAL)
jboolean ChromeHttpAuthHandler::GetIsProxyAuth(JNIEnv* env,jobject) {
#if defined(S_USE_SYSTEM_PROXY_AUTH_CREDENTIAL)
  return is_proxy_auth_;
#else
  return false;
#endif  
}

jboolean ChromeHttpAuthHandler::GetDidUseHttpAuth(JNIEnv* env, jobject) {
#if defined(S_USE_SYSTEM_PROXY_AUTH_CREDENTIAL)
  return did_use_http_auth_;
#else
  return false;
#endif
}


jint ChromeHttpAuthHandler::IsNegotiateAuthSchemePresent(
    JNIEnv* env, jobject) {
#if defined(SBROWSER_KERBEROS_FEATURE)
  if (observer_) {
    return observer_->IsNegotiatePresent();
  }
#endif
  return -1;
}

// static
bool ChromeHttpAuthHandler::RegisterChromeHttpAuthHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
