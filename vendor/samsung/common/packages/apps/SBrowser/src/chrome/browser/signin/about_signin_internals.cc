// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/about_signin_internals.h"

#include "base/debug/trace_event.h"
#include "base/hash.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_internals_util.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/common/chrome_version_info.h"
#include "google_apis/gaia/gaia_constants.h"

using base::Time;
using namespace signin_internals_util;

namespace {

std::string GetTimeStr(base::Time time) {
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

base::ListValue* AddSection(base::ListValue* parent_list,
                            const std::string& title) {
  scoped_ptr<base::DictionaryValue> section(new base::DictionaryValue());
  base::ListValue* section_contents = new base::ListValue();

  section->SetString("title", title);
  section->Set("data", section_contents);
  parent_list->Append(section.release());
  return section_contents;
}

void AddSectionEntry(base::ListValue* section_list,
                     const std::string& field_name,
                     const std::string& field_val) {
  scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
  entry->SetString("label", field_name);
  entry->SetString("value", field_val);
  section_list->Append(entry.release());
}

std::string SigninStatusFieldToLabel(UntimedSigninStatusField field) {
  switch (field) {
    case USERNAME:
      return "User Id";
    case UNTIMED_FIELDS_END:
      NOTREACHED();
      return std::string();
  }
  NOTREACHED();
  return std::string();
}

TimedSigninStatusValue SigninStatusFieldToLabel(
    TimedSigninStatusField field) {
  switch (field) {
    case SIGNIN_TYPE:
      return TimedSigninStatusValue("Type", "Time");
    case CLIENT_LOGIN_STATUS:
      return TimedSigninStatusValue("Last OnClientLogin Status",
                                    "Last OnClientLogin Time");
    case OAUTH_LOGIN_STATUS:
      return TimedSigninStatusValue("Last OnOAuthLogin Status",
                                    "Last OnOAuthLogin Time");

    case GET_USER_INFO_STATUS:
      return TimedSigninStatusValue("Last OnGetUserInfo Status",
                                    "Last OnGetUserInfo Time");
    case UBER_TOKEN_STATUS:
      return TimedSigninStatusValue("Last OnUberToken Status",
                                    "Last OnUberToken Time");
    case MERGE_SESSION_STATUS:
      return TimedSigninStatusValue("Last OnMergeSession Status",
                                    "Last OnMergeSession Time");
    case TIMED_FIELDS_END:
      NOTREACHED();
      return TimedSigninStatusValue("Error", std::string());
  }
  NOTREACHED();
  return TimedSigninStatusValue("Error", std::string());
}

// Returns a string describing the chrome version environment. Version format:
// <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
// If version information is unavailable, returns "invalid."
std::string GetVersionString() {
  chrome::VersionInfo chrome_version;
  if (!chrome_version.is_valid())
    return "invalid";
  return chrome_version.CreateVersionString();
}

}  // anonymous namespace

AboutSigninInternals::AboutSigninInternals() : profile_(NULL) {
}

AboutSigninInternals::~AboutSigninInternals() {
}

void AboutSigninInternals::AddSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.AddObserver(observer);
}

void AboutSigninInternals::RemoveSigninObserver(
    AboutSigninInternals::Observer* observer) {
  signin_observers_.RemoveObserver(observer);
}

void AboutSigninInternals::NotifySigninValueChanged(
    const UntimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - UNTIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.untimed_signin_fields.size());

  signin_status_.untimed_signin_fields[field_index] = value;

  // Also persist these values in the prefs.
  const std::string pref_path = SigninStatusFieldToString(field);
  profile_->GetPrefs()->SetString(pref_path.c_str(), value);

  NotifyObservers();
}

void AboutSigninInternals::NotifySigninValueChanged(
    const TimedSigninStatusField& field,
    const std::string& value) {
  unsigned int field_index = field - TIMED_FIELDS_BEGIN;
  DCHECK(field_index >= 0 &&
         field_index < signin_status_.timed_signin_fields.size());

  Time now = Time::NowFromSystemTime();
  std::string time_as_str =
      base::UTF16ToUTF8(base::TimeFormatFriendlyDate(now));
  TimedSigninStatusValue timed_value(value, time_as_str);

  signin_status_.timed_signin_fields[field_index] = timed_value;

  // Also persist these values in the prefs.
  const std::string value_pref = SigninStatusFieldToString(field) + ".value";
  const std::string time_pref = SigninStatusFieldToString(field) + ".time";
  profile_->GetPrefs()->SetString(value_pref.c_str(), value);
  profile_->GetPrefs()->SetString(time_pref.c_str(), time_as_str);

  NotifyObservers();
}

void AboutSigninInternals::RefreshSigninPrefs() {
  // Return if no profile exists. Can occur in unit tests.
  if (!profile_)
    return;

  PrefService* pref_service = profile_->GetPrefs();
  for (int i = UNTIMED_FIELDS_BEGIN; i < UNTIMED_FIELDS_END; ++i) {
    const std::string pref_path =
        SigninStatusFieldToString(static_cast<UntimedSigninStatusField>(i));

    signin_status_.untimed_signin_fields[i - UNTIMED_FIELDS_BEGIN] =
        pref_service->GetString(pref_path.c_str());
  }
  for (int i = TIMED_FIELDS_BEGIN ; i < TIMED_FIELDS_END; ++i) {
    const std::string value_pref = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".value";
    const std::string time_pref = SigninStatusFieldToString(
        static_cast<TimedSigninStatusField>(i)) + ".time";

    TimedSigninStatusValue value(pref_service->GetString(value_pref.c_str()),
                                 pref_service->GetString(time_pref.c_str()));
    signin_status_.timed_signin_fields[i - TIMED_FIELDS_BEGIN] = value;
  }

  // TODO(rogerta): Get status and timestamps for oauth2 tokens.

  NotifyObservers();
}

void AboutSigninInternals::Initialize(Profile* profile) {
  DCHECK(!profile_);
  profile_ = profile;

  RefreshSigninPrefs();

  SigninManagerFactory::GetForProfile(profile)->
      AddSigninDiagnosticsObserver(this);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
      AddDiagnosticsObserver(this);
}

void AboutSigninInternals::Shutdown() {
  SigninManagerFactory::GetForProfile(profile_)->
      RemoveSigninDiagnosticsObserver(this);
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)->
      RemoveDiagnosticsObserver(this);

}

void AboutSigninInternals::NotifyObservers() {
  FOR_EACH_OBSERVER(AboutSigninInternals::Observer,
                    signin_observers_,
                    OnSigninStateChanged(signin_status_.ToValue()));
}

scoped_ptr<base::DictionaryValue> AboutSigninInternals::GetSigninStatus() {
  return signin_status_.ToValue().Pass();
}

void AboutSigninInternals::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (token) {
    *token = TokenInfo(consumer_id, scopes);
  } else {
    token = new TokenInfo(consumer_id, scopes);
    signin_status_.token_info_map[account_id].push_back(token);
  }

  NotifyObservers();
}

void AboutSigninInternals::OnFetchAccessTokenComplete(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes,
    GoogleServiceAuthError error,
    base::Time expiration_time) {
  TokenInfo* token = signin_status_.FindToken(account_id, consumer_id, scopes);
  if (!token) {
    DVLOG(1) << "Can't find token: " << account_id << ", " << consumer_id;
    return;
  }

  token->receive_time = base::Time::Now();
  token->error = error;
  token->expiration_time = expiration_time;

  NotifyObservers();
}

void AboutSigninInternals::OnTokenRemoved(
    const std::string& account_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  for (size_t i = 0; i < signin_status_.token_info_map[account_id].size();
      ++i) {
    TokenInfo* token = signin_status_.token_info_map[account_id][i];
    if (token->scopes == scopes)
      token->Invalidate();
  }
  NotifyObservers();
}

AboutSigninInternals::TokenInfo::TokenInfo(
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes)
    : consumer_id(consumer_id),
      scopes(scopes),
      request_time(base::Time::Now()),
      error(GoogleServiceAuthError::AuthErrorNone()),
      removed_(false) {
}

AboutSigninInternals::TokenInfo::~TokenInfo() {}

bool AboutSigninInternals::TokenInfo::LessThan(const TokenInfo* a,
                                               const TokenInfo* b) {
  return a->consumer_id < b->consumer_id || a->scopes < b->scopes;
}

void AboutSigninInternals::TokenInfo::Invalidate() {
  removed_ = true;
}

base::DictionaryValue* AboutSigninInternals::TokenInfo::ToValue() const {
  scoped_ptr<base::DictionaryValue> token_info(new base::DictionaryValue());
  token_info->SetString("service", consumer_id);

  std::string scopes_str;
  for (OAuth2TokenService::ScopeSet::const_iterator it = scopes.begin();
      it != scopes.end(); ++it) {
    scopes_str += *it + "<br/>";
  }
  token_info->SetString("scopes", scopes_str);
  token_info->SetString("request_time", GetTimeStr(request_time).c_str());

  if (removed_) {
    token_info->SetString("status", "Token was revoked.");
  } else if (!receive_time.is_null()) {
    if (error == GoogleServiceAuthError::AuthErrorNone()) {
      bool token_expired = expiration_time < base::Time::Now();
      std::string status_str = "";
      if (token_expired)
        status_str = "<p style=\"color: #ffffff; background-color: #ff0000\">";
      base::StringAppendF(&status_str,
                          "Received token at %s. Expire at %s",
                          GetTimeStr(receive_time).c_str(),
                          GetTimeStr(expiration_time).c_str());
      if (token_expired)
        base::StringAppendF(&status_str, "</p>");
      token_info->SetString("status", status_str);
    } else {
      token_info->SetString(
          "status",
          base::StringPrintf("Failure: %s", error.error_message().c_str()));
    }
  } else {
    token_info->SetString("status", "Waiting for response");
  }

  return token_info.release();
}

AboutSigninInternals::SigninStatus::SigninStatus()
    :untimed_signin_fields(UNTIMED_FIELDS_COUNT),
     timed_signin_fields(TIMED_FIELDS_COUNT) {
}

AboutSigninInternals::SigninStatus::~SigninStatus() {
  for (TokenInfoMap::iterator it = token_info_map.begin();
      it != token_info_map.end(); ++it) {
    STLDeleteElements(&it->second);
  }
}

AboutSigninInternals::TokenInfo*
AboutSigninInternals::SigninStatus::FindToken(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  for (size_t i = 0; i < token_info_map[account_id].size();
      ++i) {
    TokenInfo* tmp = token_info_map[account_id][i];
    if (tmp->consumer_id == consumer_id && tmp->scopes == scopes)
      return tmp;
  }
  return NULL;
}

scoped_ptr<base::DictionaryValue>
AboutSigninInternals::SigninStatus::ToValue() {
  scoped_ptr<base::DictionaryValue> signin_status(new base::DictionaryValue());
  base::ListValue* signin_info = new base::ListValue();
  signin_status->Set("signin_info", signin_info);

  // A summary of signin related info first.
  base::ListValue* basic_info = AddSection(signin_info, "Basic Information");
  const std::string signin_status_string =
      untimed_signin_fields[USERNAME - UNTIMED_FIELDS_BEGIN].empty() ?
      "Not Signed In" : "Signed In";
  AddSectionEntry(basic_info, "Chrome Version", GetVersionString());
  AddSectionEntry(basic_info, "Signin Status", signin_status_string);

  // Only add username.  SID and LSID have moved to tokens section.
  const std::string field =
      SigninStatusFieldToLabel(static_cast<UntimedSigninStatusField>(USERNAME));
  AddSectionEntry(
      basic_info,
      field,
      untimed_signin_fields[USERNAME - UNTIMED_FIELDS_BEGIN]);

  // Time and status information of the possible sign in types.
  base::ListValue* detailed_info = AddSection(signin_info,
                                              "Last Signin Details");
  for (int i = TIMED_FIELDS_BEGIN; i < TIMED_FIELDS_END; ++i) {
    const std::string value_field =
        SigninStatusFieldToLabel(static_cast<TimedSigninStatusField>(i)).first;
    const std::string time_field =
        SigninStatusFieldToLabel(static_cast<TimedSigninStatusField>(i)).second;

    AddSectionEntry(detailed_info, value_field,
                    timed_signin_fields[i - TIMED_FIELDS_BEGIN].first);
    AddSectionEntry(detailed_info, time_field,
                    timed_signin_fields[i - TIMED_FIELDS_BEGIN].second);
  }

  // Token information for all services.
  base::ListValue* token_info = new base::ListValue();
  signin_status->Set("token_info", token_info);
  for (TokenInfoMap::iterator it = token_info_map.begin();
      it != token_info_map.end(); ++it) {
    base::ListValue* token_details = AddSection(token_info, it->first);

    std::sort(it->second.begin(), it->second.end(), TokenInfo::LessThan);
    const std::vector<TokenInfo*>& tokens = it->second;
    for (size_t i = 0; i < tokens.size(); ++i) {
      base::DictionaryValue* token_info = tokens[i]->ToValue();
      token_details->Append(token_info);
    }
  }

  return signin_status.Pass();
}

