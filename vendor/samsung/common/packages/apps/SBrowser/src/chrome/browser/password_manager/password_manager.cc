// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager.h"

#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_driver.h"
#include "chrome/common/pref_names.h"
#include "sbrowser/chrome/native/browser/android/sbr/preferences/sbr_pref_names.h"
#include "components/autofill/core/common/password_autofill_util.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "grit/generated_resources.h"
#include "base/logging.h"

using autofill::PasswordForm;
using autofill::PasswordFormMap;

namespace {

const char kSpdyProxyRealm[] = "/SpdyProxy";

#if defined(S_FP_SIGNUP_AUTOFILL_FIX)
//This routine checks for form which should go for autofill.
// Also can be used to decide Login Failure.
// TODO : Add/Remove attributes relavant to this.
bool DoesFormMatch(const PasswordForm& form_seen, const PasswordForm& form_stored){
   return ((form_stored.form_data.name == form_seen.form_data.name
   	         || form_stored.username_element == form_seen.username_element)
  	         && form_stored.password_element == form_seen.password_element);
}
#endif

// This routine is called when PasswordManagers are constructed.
//
// Currently we report metrics only once at startup. We require
// that this is only ever called from a single thread in order to
// avoid needing to lock (a static boolean flag is then sufficient to
// guarantee running only once).
void ReportMetrics(bool password_manager_enabled) {
  static base::PlatformThreadId initial_thread_id =
      base::PlatformThread::CurrentId();
  DCHECK(initial_thread_id == base::PlatformThread::CurrentId());

  static bool ran_once = false;
  if (ran_once)
    return;
  ran_once = true;

  UMA_HISTOGRAM_BOOLEAN("PasswordManager.Enabled", password_manager_enabled);
}

}  // namespace

const char PasswordManager::kOtherPossibleUsernamesExperiment[] =
    "PasswordManagerOtherPossibleUsernames";

// static
void PasswordManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPasswordManagerEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPasswordManagerAllowShowPasswords,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kPasswordManagerGroupsForDomains,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

PasswordManager::PasswordManager(PasswordManagerClient* client)
    : client_(client), driver_(client->GetDriver()) {
  DCHECK(client_);
  DCHECK(driver_);
  password_manager_enabled_.Init(prefs::kPasswordManagerEnabled,
                                 client_->GetPrefs());

  ReportMetrics(*password_manager_enabled_);
}

PasswordManager::~PasswordManager() {
  FOR_EACH_OBSERVER(LoginModelObserver, observers_, OnLoginModelDestroying());
}

void PasswordManager::SetFormHasGeneratedPassword(const PasswordForm& form) {
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
      (*iter)->SetHasGeneratedPassword();
      return;
    }
  }
  // If there is no corresponding PasswordFormManager, we create one. This is
  // not the common case, and should only happen when there is a bug in our
  // ability to detect forms.
  bool ssl_valid = (form.origin.SchemeIsSecure() &&
                    !driver_->DidLastPageLoadEncounterSSLErrors());
  PasswordFormManager* manager = new PasswordFormManager(
      this, client_, driver_, form, ssl_valid);
  pending_login_managers_.push_back(manager);
  manager->SetHasGeneratedPassword();
  // TODO(gcasto): Add UMA stats to track this.
}

bool PasswordManager::IsSavingEnabled() const {
  return *password_manager_enabled_ && !driver_->IsOffTheRecord();
}

void PasswordManager::ProvisionallySavePassword(const PasswordForm& form) {
  
  //LOG(INFO)<<"FP: ProvisionallySavePassword";
  if (!IsSavingEnabled()) {
  	LOG(INFO)<<"FP: ProvisionallySavePassword:SAVING_DISABLED return";
    RecordFailure(SAVING_DISABLED, form.origin.host());
    return;
  }

  // No password to save? Then don't.
  if (form.password_value.empty()) {
  	LOG(INFO)<<"FP: ProvisionallySavePassword:EMPTY_PASSWORD return";
    RecordFailure(EMPTY_PASSWORD, form.origin.host());
    return;
  }

  scoped_ptr<PasswordFormManager> manager;
  ScopedVector<PasswordFormManager>::iterator matched_manager_it =
      pending_login_managers_.end();
  for (ScopedVector<PasswordFormManager>::iterator iter =
           pending_login_managers_.begin();
       iter != pending_login_managers_.end(); ++iter) {
    // If we find a manager that exactly matches the submitted form including
    // the action URL, exit the loop.
    if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_REQUIRED)) {
      matched_manager_it = iter;
      break;
    // If the current manager matches the submitted form excluding the action
    // URL, remember it as a candidate and continue searching for an exact
    // match.
    } else if ((*iter)->DoesManage(
        form, PasswordFormManager::ACTION_MATCH_NOT_REQUIRED)) {
      matched_manager_it = iter;
    }
  }
  // If we didn't find a manager, this means a form was submitted without
  // first loading the page containing the form. Don't offer to save
  // passwords in this case.
  if (matched_manager_it != pending_login_managers_.end()) {
    // Transfer ownership of the manager from |pending_login_managers_| to
    // |manager|.
    manager.reset(*matched_manager_it);
    pending_login_managers_.weak_erase(matched_manager_it);
  } else {	
  	LOG(INFO)<<"FP: ProvisionallySavePassword:NO_MATCHING_FORM return";
    RecordFailure(NO_MATCHING_FORM, form.origin.host());
    return;
  }

  // If we found a manager but it didn't finish matching yet, the user has
  // tried to submit credentials before we had time to even find matching
  // results for the given form and autofill. If this is the case, we just
  // give up.
  if (!manager->HasCompletedMatching()) { 		
  	LOG(INFO)<<"FP: ProvisionallySavePassword:MATCHING_NOT_COMPLETE return";
    RecordFailure(MATCHING_NOT_COMPLETE, form.origin.host());
    return;
  }

  // Also get out of here if the user told us to 'never remember' passwords for
  // this form.
  if (manager->IsBlacklisted()) {		
  	LOG(INFO)<<"FP ProvisionallySavePassword:FORM_BLACKLISTED return";
    RecordFailure(FORM_BLACKLISTED, form.origin.host());
    return;
  }

  // Bail if we're missing any of the necessary form components.
  if (!manager->HasValidPasswordForm()) {
  	LOG(INFO)<<"FP: ProvisionallySavePassword:INVALID_FORM return";
    RecordFailure(INVALID_FORM, form.origin.host());
    return;
  }

  #if defined(S_AUTOCOMPLETE_IGNORE)
  bool autocompleteIgnore =  client_->GetPrefs()->GetBoolean(prefs::kWebKitAutocompleteIgnore);
  LOG(INFO)<<"FP: ProvisionallySavePassword:autocompleteIgnore ="<<autocompleteIgnore;
  if (!autocompleteIgnore && 
       !manager->HasGeneratedPassword() &&
       !form.password_autocomplete_set)
  #else
   
  // Always save generated passwords, as the user expresses explicit intent for
  // Chrome to manage such passwords. For other passwords, respect the
  // autocomplete attribute if autocomplete='off' is not ignored.
  if (!autofill::ShouldIgnoreAutocompleteOffForPasswordFields() &&
       !manager->HasGeneratedPassword() &&
       !form.password_autocomplete_set)
  #endif
  {
    RecordFailure(AUTOCOMPLETE_OFF, form.origin.host());
    // As Autocomplete is OFF for this website so showing an alert pop up to user 
  #if defined(S_AUTOCOMPLETE_ALERT_POPUP)
    LOG(INFO)<<"FP: ProvisionallySavePassword:S_AUTOCOMPLETE_ALERT Return";
    //alert toast message for autocomplete-off password forms is removed as per requirement
    //and the proper log is added to identify autocomplete off cases for weblogin.
    //client_->ShowAutoCompleteAlertPopUp();
  #endif 
    return;
  }
#if defined(S_FP_CHECKING_EMPTY_OR_INVALID_USERNAME)
  std::string username = base::UTF16ToUTF8(form.username_value);
  bool spaceCharacterFound = false;
  size_t found = username.find(" ");
  if (found != std::string::npos && found != 0 && username.size() > 0 && found != username.size()-1){
  	spaceCharacterFound = true;
  }
  if(form.username_value.empty() || spaceCharacterFound){
    RecordFailure(EMPTY_OR_INVALID_USERNAME, form.origin.host());
  //#if defined(S_AUTOCOMPLETE_ALERT_POPUP)
    //client_->ShowAutoCompleteAlertPopUp();
  //#endif 
    return;
  }
#endif
  
  PasswordForm provisionally_saved_form(form);
  provisionally_saved_form.ssl_valid =
      form.origin.SchemeIsSecure() &&
      !driver_->DidLastPageLoadEncounterSSLErrors();
  provisionally_saved_form.preferred = true;
  PasswordFormManager::OtherPossibleUsernamesAction action =
      PasswordFormManager::IGNORE_OTHER_POSSIBLE_USERNAMES;
  if (OtherPossibleUsernamesEnabled())
    action = PasswordFormManager::ALLOW_OTHER_POSSIBLE_USERNAMES;
  manager->ProvisionallySave(provisionally_saved_form, action);
  LOG(INFO)<<"FP: ProvisionallySavePassword provisional_save_manager_ **SWAP**";
  provisional_save_manager_.swap(manager);
}

void PasswordManager::RecordFailure(ProvisionalSaveFailure failure,
                                    const std::string& form_origin) {
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ProvisionalSaveFailure",
                            failure, MAX_FAILURE_VALUE);

  std::string group_name = password_manager_metrics_util::GroupIdToString(
      password_manager_metrics_util::MonitoredDomainGroupId(
          form_origin, client_->GetPrefs()));
  if (!group_name.empty()) {
    password_manager_metrics_util::LogUMAHistogramEnumeration(
        "PasswordManager.ProvisionalSaveFailure_" + group_name, failure,
        MAX_FAILURE_VALUE);
  }
}

void PasswordManager::AddSubmissionCallback(
    const PasswordSubmittedCallback& callback) {
  submission_callbacks_.push_back(callback);
}

void PasswordManager::AddObserver(LoginModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PasswordManager::RemoveObserver(LoginModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PasswordManager::DidNavigateMainFrame(bool is_in_page) {
  // Clear data after main frame navigation if the navigation was to a
  // different page.
  if (!is_in_page)
    pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormSubmitted(
    const PasswordForm& password_form) {
  ProvisionallySavePassword(password_form);
  for (size_t i = 0; i < submission_callbacks_.size(); ++i) {
    submission_callbacks_[i].Run(password_form);
  }

  pending_login_managers_.clear();
}

void PasswordManager::OnPasswordFormsParsed(
    const std::vector<PasswordForm>& forms) {
  //LOG(INFO)<<"FP:PasswordManager::OnPasswordFormsParsed size = "<<forms.size();
  // Ask the SSLManager for current security.
  bool had_ssl_error = driver_->DidLastPageLoadEncounterSSLErrors();

  for (std::vector<PasswordForm>::const_iterator iter = forms.begin();
       iter != forms.end(); ++iter) {
    // Don't involve the password manager if this form corresponds to
    // SpdyProxy authentication, as indicated by the realm.
    if (EndsWith(iter->signon_realm, kSpdyProxyRealm, true))
      continue;

    bool ssl_valid = iter->origin.SchemeIsSecure() && !had_ssl_error;
    PasswordFormManager* manager = new PasswordFormManager(
        this, client_, driver_, *iter, ssl_valid);
    pending_login_managers_.push_back(manager);
	LOG(INFO)<<"FP:PasswordManager::OnPasswordFormsParsed pending_login_managers_ CREATED ";

    // Avoid prompting the user for access to a password if they don't have
    // password saving enabled.
    PasswordStore::AuthorizationPromptPolicy prompt_policy =
        *password_manager_enabled_ ? PasswordStore::ALLOW_PROMPT
                                   : PasswordStore::DISALLOW_PROMPT;

    manager->FetchMatchingLoginsFromPasswordStore(prompt_policy);
  }
}

bool PasswordManager::ShouldPromptUserToSavePassword() const {
  return provisional_save_manager_->IsNewLogin() &&
         !provisional_save_manager_->HasGeneratedPassword() &&
         !provisional_save_manager_->IsPendingCredentialsPublicSuffixMatch();
}

void PasswordManager::OnPasswordFormsRendered(
    const std::vector<PasswordForm>& visible_forms) {
  if (!provisional_save_manager_.get()){
  	LOG(INFO)<<"FP: PasswordManager::OnPasswordFormsRendered NO provisional_save_manager_ so return";
    return;
  	}

  //LOG(INFO)<<"FP: PasswordManager::OnPasswordFormsRendered size ="<<visible_forms.size();
  DCHECK(IsSavingEnabled());

  // If we see the login form again, then the login failed.
  for (size_t i = 0; i < visible_forms.size(); ++i) {
    // TODO(vabr): The similarity check is just action equality for now. If it
    // becomes more complex, it may make sense to consider modifying and using
    // PasswordFormManager::DoesManage for it.
      #if !defined(S_FP_WRONG_POPUP_FIX)
      if (visible_forms[i].action.is_valid() &&
        provisional_save_manager_->pending_credentials().action ==
            visible_forms[i].action)
      #else
      if(DoesFormMatch(visible_forms[i], provisional_save_manager_->pending_credentials()))
      #endif
     {
      LOG(INFO)<<"FP: PasswordManager::OnPasswordFormsRendered SubmitFailed return";
      provisional_save_manager_->SubmitFailed();
      provisional_save_manager_.reset();
      return;
    }
  }

  // Looks like a successful login attempt. Either show an infobar or
  // automatically save the login data. We prompt when the user hasn't already
  // given consent, either through previously accepting the infobar or by having
  // the browser generate the password.
  provisional_save_manager_->SubmitPassed();
  if (provisional_save_manager_->HasGeneratedPassword())
    UMA_HISTOGRAM_COUNTS("PasswordGeneration.Submitted", 1);

  #if defined(S_FP_SIGNUP_POPUP_FIX)
  if (ShouldPromptUserToSavePassword() && !provisional_save_manager_->pending_credentials().is_signup_page) {
  #else
  if (ShouldPromptUserToSavePassword()){
  #endif
  	LOG(INFO)<<"FP: PasswordManager::OnPasswordFormsRendered ShouldPromptUserToSavePassword";
    client_->PromptUserToSavePassword(provisional_save_manager_.release());
  } else {
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
  }
}

void PasswordManager::PossiblyInitializeUsernamesExperiment(
    const PasswordFormMap& best_matches) const {
  if (base::FieldTrialList::Find(kOtherPossibleUsernamesExperiment))
    return;

  bool other_possible_usernames_exist = false;
  for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
       it != best_matches.end(); ++it) {
    if (!it->second->other_possible_usernames.empty()) {
      other_possible_usernames_exist = true;
      break;
    }
  }

  if (!other_possible_usernames_exist)
    return;

  const base::FieldTrial::Probability kDivisor = 100;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kOtherPossibleUsernamesExperiment,
          kDivisor,
          "Disabled",
          2013, 12, 31,
          base::FieldTrial::ONE_TIME_RANDOMIZED,
          NULL));
  base::FieldTrial::Probability enabled_probability =
      client_->GetProbabilityForExperiment(kOtherPossibleUsernamesExperiment);
  trial->AppendGroup("Enabled", enabled_probability);
}

bool PasswordManager::OtherPossibleUsernamesEnabled() const {
  return base::FieldTrialList::FindFullName(
      kOtherPossibleUsernamesExperiment) == "Enabled";
}

void PasswordManager::Autofill(
    const PasswordForm& form_for_autofill,
    const PasswordFormMap& best_matches,
    const PasswordForm& preferred_match,
    bool wait_for_username) const {
  LOG(INFO)<<"FP:PasswordManager::Autofill wait_for_username ="<<wait_for_username;

  PossiblyInitializeUsernamesExperiment(best_matches);

  // TODO(tedchoc): Switch to only requesting authentication if the user is
  //                acting on the autofilled forms (crbug.com/342594) instead
  //                of on page load.

  // If the current form's element is not autocompletable, There is no point of going forward.
  // Interesting thing is "If the form is not autocompletable, we shouldn't have stored its credential
  // in our database - So, Why are we doing it!!!
  // In Mobile: FaceBook.com If User types wrong credential Password field becomes "autocomplete=off".
  // If there is already one account is remembered, there will be FP screen but no Autofill.
  #if defined(S_AUTOCOMPLETE_IGNORE)
  bool autocompleteIgnore =  client_->GetPrefs()->GetBoolean(prefs::kWebKitAutocompleteIgnore);
  if((form_for_autofill.username_element==preferred_match.username_element) &&
  	  !form_for_autofill.password_autocomplete_set && !autocompleteIgnore)
  	return;
  #endif
  
  #if defined(S_FP_SIGNUP_AUTOFILL_FIX)
  if(!DoesFormMatch(form_for_autofill, preferred_match)){
      LOG(INFO)<<"FP:PasswordManager::Autofill : FORMS DID NOT MATCH return";
      return;
  }
  #endif

  bool authentication_required;

  if(form_for_autofill.username_element_readonly){
	authentication_required = false;
	base::string16 curr_username = base::i18n::ToLower(form_for_autofill.username_value);

	#if defined(S_FP_INVALID_EMAIL_USERNAME_FIX)
	base::string16 username_stripped_value = base::UTF8ToUTF16("");
	std::string current_username = base::UTF16ToUTF8(curr_username);
	std::size_t found = current_username.find("@");
	if(found){
            std::string s;
            s.assign(current_username, 0, found);
            username_stripped_value = base::UTF8ToUTF16(s);
	}
	#endif

	for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
		            it != best_matches.end(); ++it) {
	    // Already each username value is changed to lower case. - username_element_readonly case.
	    // We don't really need to convert it here.
	    // But Since the Guard is different for safety, doing it again
	    if(base::i18n::ToLower(it->first) == curr_username
               #if defined(S_FP_INVALID_EMAIL_USERNAME_FIX)
		 || base::i18n::ToLower(it->first) == username_stripped_value
               #endif
	        ){
		 authentication_required = it->second->use_additional_authentication;
		 break;
	    }
	}
  }else{
	authentication_required = preferred_match.use_additional_authentication;
	for (autofill::PasswordFormMap::const_iterator it = best_matches.begin();
             !authentication_required && it != best_matches.end(); ++it) {
           if (it->second->use_additional_authentication)
               authentication_required = true;
	}
  }

  LOG(INFO)<<"FP: PasswordManager::Autofill Authentication Required "<<authentication_required;
  bool manual_autofill = false;
  switch (form_for_autofill.scheme) {
    case PasswordForm::SCHEME_HTML: {
      // Note the check above is required because the observers_ for a non-HTML
      // schemed password form may have been freed, so we need to distinguish.
      scoped_ptr<autofill::PasswordFormFillData> fill_data(
          new autofill::PasswordFormFillData());
      InitPasswordFormFillData(form_for_autofill,
                               best_matches,
                               &preferred_match,
                               wait_for_username,
                               manual_autofill,
                               OtherPossibleUsernamesEnabled(),
                               fill_data.get());
      #if defined(S_FP_SUPPORT)
      fill_data.get()->authentication_required = authentication_required;
      #endif

      LOG(INFO)<<"FP: PasswordManager::Autofill MAU Size "<<fill_data.get()->additional_logins.size()<<" "<<fill_data.get()->username_list.size();
      if(authentication_required && fill_data.get()->additional_logins.size()+1 > fill_data.get()->username_list.size() && !form_for_autofill.username_element_readonly){
	  	// It Seems there are some Non-FP Accounts. Send it to Renderer Process for Manual AutoFill Only
	  	manual_autofill = true;
	       scoped_ptr<autofill::PasswordFormFillData> manual_fill_data(
                   new autofill::PasswordFormFillData());
		InitPasswordFormFillData(form_for_autofill,
                               best_matches,
                               &preferred_match,
                               wait_for_username,
                               manual_autofill,
                               OtherPossibleUsernamesEnabled(),
                               manual_fill_data.get());
		
		#if defined(S_FP_SUPPORT)
		manual_fill_data.get()->authentication_required = false;
		#endif

		// If Web SignIn is Off, Directly Autofill with first Non-FP Account.
		if(!client_->IsWebLoginEnabled())
			manual_fill_data.get()->manual_autofill = false;
		
		driver_->FillPasswordForm(*manual_fill_data.get());	
      }
      
      if (authentication_required
	      #if defined(S_FP_HIDDEN_FORM_FIX) 
	      && !form_for_autofill.is_hidden
	      #endif
         ) 
        client_->AuthenticateAutofillAndFillForm(fill_data.Pass());
      else
        driver_->FillPasswordForm(*fill_data.get());
      break;
    }
    default:
      FOR_EACH_OBSERVER(
          LoginModelObserver,
          observers_,
          OnAutofillDataAvailable(preferred_match.username_value,
                                  preferred_match.password_value));
      break;
  }

  client_->PasswordWasAutofilled(best_matches);
}

#if defined(S_FP_HIDDEN_FORM_FIX)
void PasswordManager::OnHiddenFormAutofill(const autofill::PasswordFormFillData& h_fill_data){
      LOG(INFO)<<"FP: PasswordManager::OnHiddenFormsAutofill() ";

      scoped_ptr<autofill::PasswordFormFillData> hfill_data(
             new autofill::PasswordFormFillData());
	  InitHiddenFormFillData(h_fill_data, hfill_data.get());
      client_->AuthenticateAutofillAndFillForm(hfill_data.Pass());
}
#endif

#if defined(S_FP_NEW_TAB_FIX)
void PasswordManager::OnRPPCheckBeforeTabClose(){

  if (!provisional_save_manager_.get()){
  	//LOG(INFO)<<" FP: Tab Closed";
       client_->CloseTabHere();
       return;
  }
  
  if (ShouldPromptUserToSavePassword()){
    provisional_save_manager_.get()->setTabShouldDestroy(true);
    LOG(INFO)<<"FP: PasswordManager::OnRPPCheckBeforeTabClose ShouldPromptUserToSavePassword";
    client_->PromptUserToSavePassword(provisional_save_manager_.release());
  } else {
    provisional_save_manager_->Save();
    provisional_save_manager_.reset();
    client_->CloseTabHere();
  }
}
#endif