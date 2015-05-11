// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_store.h"
#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_signin_manager_wrapper.h"
#endif
#if defined(ENABLE_SIGNIN)
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#endif
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"

ProfileSyncServiceMock::ProfileSyncServiceMock(Profile* profile)
    : ProfileSyncService(
          NULL,
          profile,
#if defined(ENABLE_MANAGED_USERS)
          new ManagedUserSigninManagerWrapper(
              SigninManagerFactory::GetForProfile(profile)),
#endif
#if defined(ENABLE_SIGNIN)
          ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
#endif
          ProfileSyncService::MANUAL_START) {}

ProfileSyncServiceMock::~ProfileSyncServiceMock() {
}

// static
TestingProfile* ProfileSyncServiceMock::MakeSignedInTestingProfile() {
  TestingProfile* profile = new TestingProfile();
  profile->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "foo");
  return profile;
}

// static
BrowserContextKeyedService* ProfileSyncServiceMock::BuildMockProfileSyncService(
    content::BrowserContext* profile) {
  return new ProfileSyncServiceMock(static_cast<Profile*>(profile));
}

ScopedVector<browser_sync::DeviceInfo>
    ProfileSyncServiceMock::GetAllSignedInDevices() const {
  ScopedVector<browser_sync::DeviceInfo> devices;
  std::vector<browser_sync::DeviceInfo*>* device_vector =
      GetAllSignedInDevicesMock();
  devices.get() = *device_vector;
  return devices.Pass();
}

scoped_ptr<browser_sync::DeviceInfo>
    ProfileSyncServiceMock::GetLocalDeviceInfo() const {
  return scoped_ptr<browser_sync::DeviceInfo>(GetLocalDeviceInfoMock()).Pass();
}
