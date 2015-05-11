// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_authentication_manager.h"

#include "chrome/browser/android/tab_android.h"
#include "jni/PasswordAuthenticationManager_jni.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

base::string16 PasswordAuthenticationManager::appSelectedUser = base::UTF8ToUTF16("");

namespace {

class PasswordAuthenticationCallback {
 public:
  explicit PasswordAuthenticationCallback(
      const base::Closure& success_callback) {
    success_callback_ = success_callback;
  }

  void OnResult(bool result,base::string16 selectedUser) {
  	
    if (result){
//	LOG(INFO)<<"WebLogin::"<<"OnResult ,selectedUser=  "<<selectedUser;
	PasswordAuthenticationManager::setSelectedUser( selectedUser);
      	success_callback_.Run();
	}
    delete this;
  }

 private:
  virtual ~PasswordAuthenticationCallback() {}

  base::Closure success_callback_;
};

}  // namespace

PasswordAuthenticationManager::PasswordAuthenticationManager() {
}

PasswordAuthenticationManager::~PasswordAuthenticationManager() {
}

bool PasswordAuthenticationManager::RegisterPasswordAuthenticationManager(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void PasswordAuthenticationManager::ShowAutoCompleteAlertPopUp(){
	  JNIEnv* env = base::android::AttachCurrentThread();
	  Java_PasswordAuthenticationManager_showAutoCompleteAlertPopUp(env);
}

void PasswordAuthenticationManager::AuthenticatePasswordAutofill(
    content::WebContents* web_contents,
    const base::Closure& success_callback,
    const std::vector<base::string16>& usernames,bool usernameReadOnly) {

	
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (!tab)
    return;
//LOG(INFO)<<" PasswordAuthenticationManager:requestAuthentication called called";
  JNIEnv* env = base::android::AttachCurrentThread();
  PasswordAuthenticationCallback* auth_callback =
      new PasswordAuthenticationCallback(success_callback);
  Java_PasswordAuthenticationManager_requestAuthentication(
      env,
      tab->GetJavaObject().obj(),
      Java_PasswordAuthenticationCallback_create(
          env,
          reinterpret_cast<intptr_t>(auth_callback)).obj(),
          base::android::ToJavaArrayOfStrings(env, usernames).obj(),usernameReadOnly);
}

// static
void OnResult(JNIEnv* env,
              jclass jcaller,
              jlong callback_ptr,
              jboolean authenticated,jstring selectedUser) {
  PasswordAuthenticationCallback* callback =
      reinterpret_cast<PasswordAuthenticationCallback*>(callback_ptr);
  callback->OnResult(authenticated,base::android::ConvertJavaStringToUTF16(env,selectedUser));
}

bool PasswordAuthenticationManager::IsWebSignInEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  bool result = Java_PasswordAuthenticationManager_isWebSignInEnabled(env);
  LOG(INFO) << " WebLogin: FP setting is = " << result;
  return result;
}
