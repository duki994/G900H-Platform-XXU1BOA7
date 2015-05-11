// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/content_password_manager_driver.h"

#include "components/autofill/content/browser/autofill_driver_impl.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/ssl_status.h"
#include "ipc/ipc_message_macros.h"
#include "net/cert/cert_status_flags.h"

ContentPasswordManagerDriver::ContentPasswordManagerDriver(
    content::WebContents* web_contents,
    PasswordManagerClient* client)
    : WebContentsObserver(web_contents),
      password_manager_(client),
      password_generation_manager_(web_contents, client) {
  DCHECK(web_contents);
}

ContentPasswordManagerDriver::~ContentPasswordManagerDriver() {}

void ContentPasswordManagerDriver::FillPasswordForm(
    const autofill::PasswordFormFillData& form_data) {
#if defined(ENABLE_AUTOFILL)
  DCHECK(web_contents());
  web_contents()->GetRenderViewHost()->Send(new AutofillMsg_FillPasswordForm(
      web_contents()->GetRenderViewHost()->GetRoutingID(), form_data));
#endif
}

bool ContentPasswordManagerDriver::DidLastPageLoadEncounterSSLErrors() {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  if (!entry) {
    NOTREACHED();
    return false;
  }

  return net::IsCertStatusError(entry->GetSSL().cert_status);
}

bool ContentPasswordManagerDriver::IsOffTheRecord() {
  DCHECK(web_contents());
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

PasswordGenerationManager*
ContentPasswordManagerDriver::GetPasswordGenerationManager() {
  return &password_generation_manager_;
}

PasswordManager* ContentPasswordManagerDriver::GetPasswordManager() {
  return &password_manager_;
}

void ContentPasswordManagerDriver::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  password_manager_.DidNavigateMainFrame(details.is_in_page);
}

bool ContentPasswordManagerDriver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordManager, message)
#if defined(ENABLE_AUTOFILL)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PasswordFormsParsed,
                      &password_manager_,
                      PasswordManager::OnPasswordFormsParsed)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PasswordFormsRendered,
                      &password_manager_,
                      PasswordManager::OnPasswordFormsRendered)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_PasswordFormSubmitted,
                      &password_manager_,
                      PasswordManager::OnPasswordFormSubmitted)
  #if defined(S_FP_HIDDEN_FORM_FIX)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_HiddenFormAutofill,
                      &password_manager_,
                      PasswordManager::OnHiddenFormAutofill)
  #endif  

  #if defined(S_FP_NEW_TAB_FIX)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_RPPCheckBeforeTabClose,
                      &password_manager_,
                      PasswordManager::OnRPPCheckBeforeTabClose)
  #endif
  
  IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowPasswordGenerationPopup,
                      &password_generation_manager_,
                      PasswordGenerationManager::OnShowPasswordGenerationPopup)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_ShowPasswordEditingPopup,
                      &password_generation_manager_,
                      PasswordGenerationManager::OnShowPasswordEditingPopup)
  IPC_MESSAGE_FORWARD(AutofillHostMsg_HidePasswordGenerationPopup,
                      &password_generation_manager_,
                      PasswordGenerationManager::OnHidePasswordGenerationPopup)
#endif
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

autofill::AutofillManager* ContentPasswordManagerDriver::GetAutofillManager() {
#if defined(ENABLE_AUTOFILL)
  autofill::AutofillDriverImpl* driver =
      autofill::AutofillDriverImpl::FromWebContents(web_contents());
  return driver ? driver->autofill_manager() : NULL;
#else
  return NULL;
#endif
}

void ContentPasswordManagerDriver::AllowPasswordGenerationForForm(
    autofill::PasswordForm* form) {
#if defined(ENABLE_AUTOFILL)
  content::RenderViewHost* host = web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_FormNotBlacklisted(host->GetRoutingID(), *form));
#endif
}
