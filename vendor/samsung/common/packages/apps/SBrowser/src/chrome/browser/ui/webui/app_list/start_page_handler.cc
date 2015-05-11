// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_list/start_page_handler.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/recommended_apps.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/speech_ui_model_observer.h"
#include "ui/events/event_constants.h"

namespace app_list {

namespace {

scoped_ptr<base::DictionaryValue> CreateAppInfo(
    const extensions::Extension* app) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("appId", app->id());
  dict->SetString("textTitle", app->short_name());
  dict->SetString("title", app->name());

  const bool grayscale = false;
  bool icon_exists = true;
  GURL icon_url = extensions::ExtensionIconSource::GetIconURL(
      app,
      extension_misc::EXTENSION_ICON_MEDIUM,
      ExtensionIconSet::MATCH_BIGGER,
      grayscale,
      &icon_exists);
  dict->SetString("iconUrl", icon_url.spec());

  return dict.Pass();
}

}  // namespace

StartPageHandler::StartPageHandler() : recommended_apps_(NULL) {}

StartPageHandler::~StartPageHandler() {
  if (recommended_apps_)
    recommended_apps_->RemoveObserver(this);
}

void StartPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::Bind(&StartPageHandler::HandleInitialize, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchApp",
      base::Bind(&StartPageHandler::HandleLaunchApp, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "speechResult",
      base::Bind(&StartPageHandler::HandleSpeechResult,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "speechSoundLevel",
      base::Bind(&StartPageHandler::HandleSpeechSoundLevel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setSpeechRecognitionState",
      base::Bind(&StartPageHandler::HandleSpeechRecognition,
                 base::Unretained(this)));
}

void StartPageHandler::OnRecommendedAppsChanged() {
  SendRecommendedApps();
}

void StartPageHandler::SendRecommendedApps() {
  const RecommendedApps::Apps& recommends = recommended_apps_->apps();

  base::ListValue recommended_list;
  for (size_t i = 0; i < recommends.size(); ++i) {
    recommended_list.Append(CreateAppInfo(recommends[i].get()).release());
  }

  web_ui()->CallJavascriptFunction("appList.startPage.setRecommendedApps",
                                   recommended_list);
}

#if defined(OS_CHROMEOS)
bool StartPageHandler::HotwordEnabled() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return HotwordService::DoesHotwordSupportLanguage(profile) &&
      profile->GetPrefs()->GetBoolean(prefs::kHotwordAppListEnabled);
}

void StartPageHandler::OnHotwordEnabledChanged() {
  web_ui()->CallJavascriptFunction(
      "appList.startPage.setHotwordEnabled",
      base::FundamentalValue(HotwordEnabled()));
}

void StartPageHandler::SynchronizeHotwordEnabled() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* pref_service = profile->GetPrefs();
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kHotwordSearchEnabled);
  if (!pref || pref->IsDefaultValue())
    return;

  bool search_enabled = false;
  if (!pref->GetValue()->GetAsBoolean(&search_enabled))
    return;

  if (pref_service->GetBoolean(prefs::kHotwordAppListEnabled) != search_enabled)
    pref_service->SetBoolean(prefs::kHotwordAppListEnabled, search_enabled);
}
#endif

void StartPageHandler::HandleInitialize(const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  StartPageService* service = StartPageService::Get(profile);
  if (!service)
    return;

  recommended_apps_ = service->recommended_apps();
  recommended_apps_->AddObserver(this);

  SendRecommendedApps();

#if defined(OS_CHROMEOS)
  if (app_list::switches::IsVoiceSearchEnabled() &&
      HotwordService::DoesHotwordSupportLanguage(profile) &&
      base::SysInfo::IsRunningOnChromeOS()) {
    SynchronizeHotwordEnabled();
    OnHotwordEnabledChanged();
    pref_change_registrar_.Init(profile->GetPrefs());
    pref_change_registrar_.Add(
        prefs::kHotwordSearchEnabled,
        base::Bind(&StartPageHandler::SynchronizeHotwordEnabled,
                   base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kHotwordAppListEnabled,
        base::Bind(&StartPageHandler::OnHotwordEnabledChanged,
                   base::Unretained(this)));
  }
#endif
}

void StartPageHandler::HandleLaunchApp(const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  Profile* profile = Profile::FromWebUI(web_ui());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* app = service->GetInstalledExtension(app_id);
  if (!app) {
    NOTREACHED();
    return;
  }

  AppListControllerDelegate* controller = AppListService::Get(
      chrome::GetHostDesktopTypeForNativeView(
          web_ui()->GetWebContents()->GetView()->GetNativeView()))
      ->GetControllerDelegate();
  controller->ActivateApp(profile,
                          app,
                          AppListControllerDelegate::LAUNCH_FROM_APP_LIST,
                          ui::EF_NONE);
}

void StartPageHandler::HandleSpeechResult(const base::ListValue* args) {
  base::string16 query;
  bool is_final = false;
  CHECK(args->GetString(0, &query));
  CHECK(args->GetBoolean(1, &is_final));

  StartPageService::Get(Profile::FromWebUI(web_ui()))->OnSpeechResult(
      query, is_final);
}

void StartPageHandler::HandleSpeechSoundLevel(const base::ListValue* args) {
  double level;
  CHECK(args->GetDouble(0, &level));

  StartPageService* service =
      StartPageService::Get(Profile::FromWebUI(web_ui()));
  if (service)
    service->OnSpeechSoundLevelChanged(static_cast<int16>(level));
}

void StartPageHandler::HandleSpeechRecognition(const base::ListValue* args) {
  std::string state_string;
  CHECK(args->GetString(0, &state_string));

  SpeechRecognitionState new_state = SPEECH_RECOGNITION_OFF;
  if (state_string == "READY")
    new_state = SPEECH_RECOGNITION_READY;
  else if (state_string == "HOTWORD_RECOGNIZING")
    new_state = SPEECH_RECOGNITION_HOTWORD_LISTENING;
  else if (state_string == "RECOGNIZING")
    new_state = SPEECH_RECOGNITION_RECOGNIZING;
  else if (state_string == "IN_SPEECH")
    new_state = SPEECH_RECOGNITION_IN_SPEECH;
  else if (state_string == "STOPPING")
    new_state = SPEECH_RECOGNITION_STOPPING;

  StartPageService* service =
      StartPageService::Get(Profile::FromWebUI(web_ui()));
  if (service)
    service->OnSpeechRecognitionStateChanged(new_state);
}

}  // namespace app_list
