// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_default.h"

#include <set>

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "components/password_manager/core/browser/password_store_change.h"

using autofill::PasswordForm;

PasswordStoreDefault::PasswordStoreDefault(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
    LoginDatabase* login_db)
    : PasswordStore(main_thread_runner, db_thread_runner), login_db_(login_db) {
  DCHECK(login_db);
}

PasswordStoreDefault::~PasswordStoreDefault() {
}

void PasswordStoreDefault::ReportMetricsImpl() {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  login_db_->ReportMetrics();
}

PasswordStoreChangeList PasswordStoreDefault::AddLoginImpl(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  if (login_db_->AddLogin(form))
    changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::UpdateLoginImpl(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  if (login_db_->UpdateLogin(form, NULL))
    changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginImpl(
    const PasswordForm& form) {
  PasswordStoreChangeList changes;
  if (login_db_->RemoveLogin(form))
    changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
  return changes;
}

PasswordStoreChangeList PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(
    const base::Time& delete_begin, const base::Time& delete_end) {
  std::vector<PasswordForm*> forms;
  PasswordStoreChangeList changes;
  if (login_db_->GetLoginsCreatedBetween(delete_begin, delete_end, &forms)) {
    if (login_db_->RemoveLoginsCreatedBetween(delete_begin, delete_end)) {
      for (std::vector<PasswordForm*>::const_iterator it = forms.begin();
           it != forms.end(); ++it) {
        changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE,
                                              **it));
      }
      LogStatsForBulkDeletion(changes.size());
    }
  }
  STLDeleteElements(&forms);
  return changes;
}

void PasswordStoreDefault::GetLoginsImpl(
    const autofill::PasswordForm& form,
    AuthorizationPromptPolicy prompt_policy,
    const ConsumerCallbackRunner& callback_runner) {
  std::vector<PasswordForm*> matched_forms;
  login_db_->GetLogins(form, &matched_forms);
  callback_runner.Run(matched_forms);
}

void PasswordStoreDefault::GetAutofillableLoginsImpl(
    GetLoginsRequest* request) {
  FillAutofillableLogins(request->result());
  ForwardLoginsResult(request);
}

void PasswordStoreDefault::GetBlacklistLoginsImpl(
    GetLoginsRequest* request) {
  FillBlacklistLogins(request->result());
  ForwardLoginsResult(request);
}

bool PasswordStoreDefault::FillAutofillableLogins(
         std::vector<PasswordForm*>* forms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_db_->GetAutofillableLogins(forms);
}

bool PasswordStoreDefault::FillBlacklistLogins(
         std::vector<PasswordForm*>* forms) {
  DCHECK(GetBackgroundTaskRunner()->BelongsToCurrentThread());
  return login_db_->GetBlacklistLogins(forms);
}
