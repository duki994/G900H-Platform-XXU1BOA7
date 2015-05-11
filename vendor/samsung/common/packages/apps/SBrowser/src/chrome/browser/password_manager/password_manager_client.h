// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CLIENT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CLIENT_H_

#include "base/metrics/field_trial.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"

class PasswordFormManager;
class PasswordManagerDriver;
class PasswordStore;
class PrefService;

// An abstraction of operations that depend on the embedders (e.g. Chrome)
// environment.
class PasswordManagerClient {
 public:
  PasswordManagerClient() {}
  virtual ~PasswordManagerClient() {}

  // Informs the embedder of a password form that can be saved if the user
  // allows it. The embedder is not required to prompt the user if it decides
  // that this form doesn't need to be saved.
  virtual void PromptUserToSavePassword(PasswordFormManager* form_to_save) = 0;

  // Called when a password is autofilled. Default implementation is a no-op.
  virtual void PasswordWasAutofilled(
      const autofill::PasswordFormMap& best_matches) const {}

  // Called to authenticate the autofill password data.  If authentication is
  // successful, this should continue filling the form.
  virtual void AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) = 0;

  #if defined(S_FP_NEW_TAB_FIX)
  virtual void CloseTabHere() { }
  #endif

  // returns true if Settings->Finger Scanner -> Web SignIn is enabled.
  // Overridden in chrome_password_manager_client
  // the default implementation returns false
  virtual bool IsWebLoginEnabled() { return false; }
  
  // Gets prefs associated with this embedder.
  virtual PrefService* GetPrefs() = 0;

  // Returns the PasswordStore associated with this instance.
  virtual PasswordStore* GetPasswordStore() = 0;

  // Returns the PasswordManagerDriver instance associated with this instance.
  virtual PasswordManagerDriver* GetDriver() = 0;

  // Returns the probability that the experiment identified by |experiment_name|
  // should be enabled. The default implementation returns 0.
  virtual base::FieldTrial::Probability GetProbabilityForExperiment(
      const std::string& experiment_name);

#if defined(ENABLE_SYNC)
  // Returns true if password sync is enabled in the embedder. The default
  // implementation returns false.
  virtual bool IsPasswordSyncEnabled();
#endif

  // To show Alert Pop up to user incase the website has AutoComplete Flag set as OFF
  #if defined(S_AUTOCOMPLETE_ALERT_POPUP)
  virtual void ShowAutoCompleteAlertPopUp() = 0;
  #endif
 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerClient);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_CLIENT_H_
