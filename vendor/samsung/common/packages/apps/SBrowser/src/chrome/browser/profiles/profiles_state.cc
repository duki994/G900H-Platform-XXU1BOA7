// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif

namespace profiles {

bool IsMultipleProfilesEnabled() {
#if defined(OS_ANDROID)
  return false;
#endif
#if defined(OS_CHROMEOS)
  return chromeos::UserManager::IsMultipleProfilesAllowed();
#endif

  return true;
}

base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir) {
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kInitialProfile);
  return default_profile_dir;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kProfileLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
  registry->RegisterListPref(prefs::kProfilesLastActive);
}

base::string16 GetActiveProfileDisplayName(Browser* browser) {
  base::string16 profile_name;
  Profile* profile = browser->profile();

  if (profile->IsGuestSession()) {
    profile_name = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  } else {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
    if (index != std::string::npos)
      profile_name = cache.GetNameOfProfileAtIndex(index);
  }
  return profile_name;
}

void UpdateProfileName(Profile* profile,
                       const base::string16& new_profile_name) {
  ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
  base::FilePath profile_file_path = profile->GetPath();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);

  if ((new_profile_name ==
          cache.GetGAIAGivenNameOfProfileAtIndex(profile_index)) ||
      (new_profile_name == cache.GetGAIANameOfProfileAtIndex(profile_index))) {
    // Set the profile to use the GAIA name as the profile name. Note, this
    // is a little weird if the user typed their GAIA name manually but
    // it's not a big deal.
    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, true);
  } else {
    PrefService* pref_service = profile->GetPrefs();
    // Updating the profile preference will cause the cache to be updated for
    // this preference.
    pref_service->SetString(prefs::kProfileName,
                            base::UTF16ToUTF8(new_profile_name));

    // Changing the profile name can invalidate the profile index.
    profile_index = cache.GetIndexOfProfileWithPath(profile_file_path);
    if (profile_index == std::string::npos)
      return;

    cache.SetIsUsingGAIANameOfProfileAtIndex(profile_index, false);
  }
}

std::vector<std::string> GetSecondaryAccountsForProfile(
    Profile* profile,
    const std::string& primary_account) {
  std::vector<std::string> accounts =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->GetAccounts();

  // The vector returned by ProfileOAuth2TokenService::GetAccounts() contains
  // the primary account too, so we need to remove it from the list.
  std::vector<std::string>::iterator primary_index =
      std::find_if(accounts.begin(), accounts.end(),
                   std::bind1st(std::equal_to<std::string>(), primary_account));
  DCHECK(primary_index != accounts.end());
  accounts.erase(primary_index);

  return accounts;
}

bool IsRegularOrGuestSession(Browser* browser) {
  Profile* profile = browser->profile();
  return profile->IsGuestSession() || !profile->IsOffTheRecord();
}

}  // namespace profiles
