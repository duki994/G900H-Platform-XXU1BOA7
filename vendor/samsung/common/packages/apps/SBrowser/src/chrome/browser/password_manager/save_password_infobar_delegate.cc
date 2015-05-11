// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"

#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/profile_management_switches.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
void SavePasswordInfoBarDelegate::Create(
    content::WebContents* web_contents,
    PasswordFormManager* form_to_save,
    const std::string& uma_histogram_suffix) {
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // Don't show the password manager infobar if this form is for a google
  // account and we are going to show the one-click signin infobar.
  GURL realm(form_to_save->realm());
  // TODO(mathp): Checking only against associated_username() causes a bug
  // referenced here: crbug.com/133275
  // TODO(vabr): The check IsEnableWebBasedSignin is a hack for the time when
  // OneClickSignin is disabled. http://crbug.com/339804
  if (((realm == GaiaUrls::GetInstance()->gaia_login_form_realm()) ||
       (realm == GURL("https://www.google.com/"))) &&
      switches::IsEnableWebBasedSignin() &&
      OneClickSigninHelper::CanOffer(
          web_contents,
          OneClickSigninHelper::CAN_OFFER_FOR_INTERSTITAL_ONLY,
          base::UTF16ToUTF8(form_to_save->associated_username()),
          NULL))
    return;
#endif

  InfoBarService::FromWebContents(web_contents)->AddInfoBar(
      SavePasswordInfoBarDelegate::CreateInfoBar(
          scoped_ptr<SavePasswordInfoBarDelegate>(
              new SavePasswordInfoBarDelegate(form_to_save,
                                              uma_histogram_suffix))));
}

SavePasswordInfoBarDelegate::~SavePasswordInfoBarDelegate() {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.InfoBarResponse",
                            infobar_response_, NUM_RESPONSE_TYPES);

  // The shortest period for which the prompt needs to live, so that we don't
  // consider it killed prematurely, as might happen, e.g., if a pre-rendered
  // page gets swapped in (and the current WebContents is destroyed).
  const base::TimeDelta kMinimumPromptDisplayTime =
      base::TimeDelta::FromSeconds(1);

  if (!uma_histogram_suffix_.empty()) {
    password_manager_metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.SavePasswordPromptResponse_" + uma_histogram_suffix_,
        infobar_response_, NUM_RESPONSE_TYPES);
    password_manager_metrics_util::LogUMAHistogramBoolean(
        "PasswordManager.SavePasswordPromptDisappearedQuickly_" +
            uma_histogram_suffix_,
        timer_.Elapsed() < kMinimumPromptDisplayTime);
  }
}

void SavePasswordInfoBarDelegate::SetUseAdditionalPasswordAuthentication(
    bool use_additional_authentication) {
  form_to_save_->SetUseAdditionalPasswordAuthentication(
      use_additional_authentication);
}

SavePasswordInfoBarDelegate::SavePasswordInfoBarDelegate(
    PasswordFormManager* form_to_save,
    const std::string& uma_histogram_suffix)
    : ConfirmInfoBarDelegate(),
      form_to_save_(form_to_save),
      infobar_response_(NO_RESPONSE),
      uma_histogram_suffix_(uma_histogram_suffix) {
  if (!uma_histogram_suffix_.empty()) {
    password_manager_metrics_util::LogUMAHistogramBoolean(
        "PasswordManager.SavePasswordPromptDisplayed_" + uma_histogram_suffix_,
        true);
  }
}

#if !defined(OS_ANDROID)
// On Android, the save password infobar supports an additional checkbox to
// require additional authentication before autofilling a saved password.
// Because of this non-standard UI, the Android version is special cased and
// constructed in:
// chrome/browser/ui/android/infobars/save_password_infobar.cc

// static
scoped_ptr<InfoBar> SavePasswordInfoBarDelegate::CreateInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate) {
  return ConfirmInfoBarDelegate::CreateInfoBar(
      delegate.PassAs<ConfirmInfoBarDelegate>());
}
#endif

bool SavePasswordInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  bool is_not_redirect = !(details.entry->GetTransitionType() &
                           content::PAGE_TRANSITION_IS_REDIRECT_MASK);
  return is_not_redirect && InfoBarDelegate::ShouldExpire(details);
}

int SavePasswordInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_SAVE_PASSWORD;
}

InfoBarDelegate::Type SavePasswordInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

base::string16 SavePasswordInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_PASSWORD_PROMPT);
}

base::string16 SavePasswordInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PASSWORD_MANAGER_SAVE_BUTTON : IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON);
}

bool SavePasswordInfoBarDelegate::Accept() {
  DCHECK(form_to_save_.get());
  form_to_save_->Save();
  infobar_response_ = REMEMBER_PASSWORD;
  #if defined(S_FP_NEW_TAB_FIX)
  if(form_to_save_->tabShouldDestroy()){
  	   //LOG(INFO)<<"FP Destroying tab here";
          form_to_save_->DestroyTab();
  }
  #endif
  return true;
}

bool SavePasswordInfoBarDelegate::Cancel() {
  DCHECK(form_to_save_.get());
  form_to_save_->PermanentlyBlacklist();
  infobar_response_ = NEVER_REMEMBER_PASSWORD;
  #if defined(S_FP_NEW_TAB_FIX)
  if(form_to_save_->tabShouldDestroy()){
  	   //LOG(INFO)<<"FP Destroying tab here";
          form_to_save_->DestroyTab();
  }
  #endif
  return true;
}

void SavePasswordInfoBarDelegate::InfoBarDismissed() {
  DCHECK(form_to_save_.get());
  infobar_response_ = INFOBAR_DISMISSED;
  #if defined(S_FP_NEW_TAB_FIX)
  if(form_to_save_->tabShouldDestroy()){
  	   //LOG(INFO)<<"FP Destroying tab here";
          form_to_save_->DestroyTab();
  }
  #endif
}

InfoBarDelegate::InfoBarAutomationType
    SavePasswordInfoBarDelegate::GetInfoBarAutomationType() const {
  return PASSWORD_INFOBAR;
}
