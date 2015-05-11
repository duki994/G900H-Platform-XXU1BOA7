// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

void TestAuthErrorCallback(const GoogleServiceAuthError& error) {}

class ManagedUserServiceTestManaged : public InProcessBrowserTest {
 public:
  // content::BrowserTestBase:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kManagedUserId, "asdf");
  }
};

}  // namespace

typedef InProcessBrowserTest ManagedUserServiceTest;

// Crashes on Mac.
// http://crbug.com/339501
#if defined(OS_MACOSX)
#define MAYBE_ClearOmitOnRegistration DISABLED_ClearOmitOnRegistration
#else
#define MAYBE_ClearOmitOnRegistration ClearOmitOnRegistration
#endif
// Ensure that a profile that has completed registration is included in the
// list shown in the avatar menu.
IN_PROC_BROWSER_TEST_F(ManagedUserServiceTest, MAYBE_ClearOmitOnRegistration) {
  // Artificially mark the profile as omitted.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  Profile* profile = browser()->profile();
  size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  cache.SetIsOmittedProfileAtIndex(index, true);
  ASSERT_TRUE(cache.IsOmittedProfileAtIndex(index));

  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);

  // A registration error does not clear the flag (the profile should be deleted
  // anyway).
  managed_user_service->OnManagedUserRegistered(
      base::Bind(&TestAuthErrorCallback),
      profile,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
      std::string());
  ASSERT_TRUE(cache.IsOmittedProfileAtIndex(index));

  // Successfully completing registration clears the flag.
  managed_user_service->OnManagedUserRegistered(
      base::Bind(&TestAuthErrorCallback),
      profile,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      std::string("abcdef"));
  EXPECT_FALSE(cache.IsOmittedProfileAtIndex(index));
}

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceSafeSearch));
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kForceSafeSearch));
}

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTest, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kProfileName));

  std::string original_name = prefs->GetString(prefs::kProfileName);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(original_name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
}

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTestManaged, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceSafeSearch));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(prefs::kForceSafeSearch));
}

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTestManaged, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  std::string original_name = prefs->GetString(prefs::kProfileName);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  ManagedUserSettingsService* settings =
      ManagedUserSettingsServiceFactory::GetForProfile(profile);

  std::string name = "Managed User Test Name";
  settings->SetLocalSettingForTesting(
      managed_users::kUserName,
      scoped_ptr<base::Value>(new base::StringValue(name)));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(prefs::kProfileName));
  EXPECT_EQ(name, prefs->GetString(prefs::kProfileName));
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Change the name once more.
  std::string new_name = "New Managed User Test Name";
  settings->SetLocalSettingForTesting(
      managed_users::kUserName,
      scoped_ptr<base::Value>(new base::StringValue(new_name)));
  EXPECT_EQ(new_name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(new_name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Remove the setting.
  settings->SetLocalSettingForTesting(managed_users::kUserName,
                                      scoped_ptr<base::Value>());
  EXPECT_EQ(original_name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(original_name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
}
