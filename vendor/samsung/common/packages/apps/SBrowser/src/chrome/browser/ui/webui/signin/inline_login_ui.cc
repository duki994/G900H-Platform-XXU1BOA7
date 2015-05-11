// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_ui.h"

#include "chrome/browser/extensions/extension_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/login/inline_login_handler_chromeos.h"
#else
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"
#endif

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIChromeSigninHost);
  source->OverrideContentSecurityPolicyFrameSrc("frame-src chrome-extension:;");
  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_INLINE_LOGIN_HTML);
  source->AddResourcePath("inline_login.css", IDR_INLINE_LOGIN_CSS);
  source->AddResourcePath("inline_login.js", IDR_INLINE_LOGIN_JS);

  source->AddLocalizedString("title", IDS_CHROME_SIGNIN_TITLE);
  return source;
};

} // empty namespace

InlineLoginUI::InlineLoginUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui),
      auth_extension_(Profile::FromWebUI(web_ui)) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());

#if defined(OS_CHROMEOS)
  web_ui->AddMessageHandler(new chromeos::InlineLoginHandlerChromeOS());
#else
  web_ui->AddMessageHandler(new InlineLoginHandlerImpl());
  // Required for intercepting extension function calls when the page is loaded
  // in a bubble (not a full tab, thus tab helpers are not registered
  // automatically).
  extensions::ExtensionWebContentsObserver::CreateForWebContents(
      web_ui->GetWebContents());
#endif
}

InlineLoginUI::~InlineLoginUI() {}
