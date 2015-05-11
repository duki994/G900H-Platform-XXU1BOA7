

// Defines various defaults for s-browser application .

#ifndef CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_DEFAULTS_H_
#define CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_DEFAULTS_H_

#include "base/strings/string16.h"

namespace browser_defaults {

extern const bool kRememberFormDataEnabled;
extern const bool kAutofillEnabled;
extern const bool kResolveNavigationErrorEnabled;
extern const bool kSearchSuggestEnabled;
extern const bool kNetworkPredictionEnabled;
extern const float kFontScaleFactor;
extern const bool kForceZoomEnabled;
extern const bool kJavascriptEnabled;
extern const bool kPopupEnabled;
extern const bool kCookiesEnabled;
extern const bool kLocationEnabled;
extern const bool kRememberPasswordEnabled;
extern const bool kLoadImagesAutomaticallyEnabled;
extern const std::string kCharset;
extern const int  kSearchEngine;
extern const int kHomeScreenMode;
extern const bool kFontBoostingModeEnabled;
extern const int kImideoDebugMode;

}  // namespace browser_defaults

#endif  // CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_DEFAULTS_H_
