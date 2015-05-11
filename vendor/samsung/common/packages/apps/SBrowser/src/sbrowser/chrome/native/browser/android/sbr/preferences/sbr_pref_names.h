

#ifndef CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PREF_NAMES
#define CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PREF_NAMES

#include <stddef.h>
#include "build/build_config.h"

namespace prefs {
extern const char kWebkitOverviewModeEnabled[];
extern const char kWebkitHomeScreenMode[];
extern const char kBandwidthConservationSetting[];
// Bing Search Engine Changes
extern const char kWebKitIsBingAsDefaultSearchEngine[];
// Save page feature
extern const char kWebKitAllowContentURLAccess[];
// For Font Boosting Feature
extern const char kWebKitFontBoostingModeEnabled[];
#if defined(S_AUTOCOMPLETE_IGNORE)
extern const char kWebKitAutocompleteIgnore[];
#endif
// For Imideo Feature
extern const char kWebKitImideoDebugMode[];
//HTML5 Web Notification Feature
extern const char kWebNotificationType[];
} // namespace prefs

#endif // CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_PREF_NAMES
