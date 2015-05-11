// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.




#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_GEOLOCATION_SBR_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_GEOLOCATION_SBR_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "chrome/browser/android/google_location_settings_helper.h"

// Stub implementation of GoogleLocationSettingsHelper for Sbrowser.
class SbrGoogleLocationSettingsHelperImpl
    : public GoogleLocationSettingsHelper {
public:
  // GoogleLocationSettingsHelper implementation:
  virtual std::string GetAcceptButtonLabel() OVERRIDE;
  virtual void ShowGoogleLocationSettings() OVERRIDE;
  virtual bool IsMasterLocationSettingEnabled() OVERRIDE;
  virtual bool IsGoogleAppsLocationSettingEnabled() OVERRIDE;

protected:
  SbrGoogleLocationSettingsHelperImpl();
  virtual ~SbrGoogleLocationSettingsHelperImpl();

private:
  friend class GoogleLocationSettingsHelper;

  DISALLOW_COPY_AND_ASSIGN(SbrGoogleLocationSettingsHelperImpl);
};

#endif  // SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_GEOLOCATION_SBR_GOOGLE_LOCATION_SETTINGS_HELPER_H_
