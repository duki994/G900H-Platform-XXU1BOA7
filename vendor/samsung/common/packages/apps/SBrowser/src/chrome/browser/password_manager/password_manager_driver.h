// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DRIVER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DRIVER_H_

class PasswordGenerationManager;
class PasswordManager;

namespace autofill {
class AutofillManager;
struct PasswordForm;
struct PasswordFormFillData;
}  // namespace autofill

// Interface that allows PasswordManager core code to interact with its driver
// (i.e., obtain information from it and give information to it).
class PasswordManagerDriver {
 public:
  PasswordManagerDriver() {}
  virtual ~PasswordManagerDriver() {}

  // Fills forms matching |form_data|.
  virtual void FillPasswordForm(
      const autofill::PasswordFormFillData& form_data) = 0;

  // Returns whether any SSL certificate errors were encountered as a result of
  // the last page load.
  virtual bool DidLastPageLoadEncounterSSLErrors() = 0;

  // If this browsing session should not be persisted.
  virtual bool IsOffTheRecord() = 0;

  // Returns the PasswordGenerationManager associated with this instance.
  virtual PasswordGenerationManager* GetPasswordGenerationManager() = 0;

  // Returns the PasswordManager associated with this instance.
  virtual PasswordManager* GetPasswordManager() = 0;

  // Returns the AutofillManager associated with this instance.
  virtual autofill::AutofillManager* GetAutofillManager() = 0;

  // Informs the driver that |form| can be used for password generation.
  virtual void AllowPasswordGenerationForForm(autofill::PasswordForm* form) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordManagerDriver);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_DRIVER_H_
