// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_AUTHENTICATION_MANAGER_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_AUTHENTICATION_MANAGER_H_

#include "base/android/jni_android.h"
#include "base/callback.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "base/strings/utf_string_conversions.h"

namespace content {
class WebContents;
}

// Manager for authenticating the use of stored passwords.
class PasswordAuthenticationManager {
 public:

	static void setSelectedUser(base::string16 selectedUser )
	{
		appSelectedUser = selectedUser;
	}
	static base::string16 getSelectedUser()
	{
		return appSelectedUser;
	}
  // JNI registration
  static bool RegisterPasswordAuthenticationManager(JNIEnv* env);

  // Request an authentication challenge for the specified webcontents to allow
  // password autofill.  If the authentication is successful, run the
  // |success_callback|.
  static void AuthenticatePasswordAutofill(
      content::WebContents* web_contents,
      const base::Closure& success_callback,
       const std::vector<base::string16>& usernames,bool usernameReadOnly);
  // To Show an Alert Pop up to user incase Autocomplete is OFF for this website
  static void ShowAutoCompleteAlertPopUp();
  static bool IsWebSignInEnabled();
 private:
  static base::string16 appSelectedUser;
  PasswordAuthenticationManager();
  ~PasswordAuthenticationManager();

  DISALLOW_COPY_AND_ASSIGN(PasswordAuthenticationManager);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_AUTHENTICATION_MANAGER_H_
