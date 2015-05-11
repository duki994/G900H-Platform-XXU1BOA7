// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_util.h"

#if defined(ENABLE_EXTENSIONS_ALL)  
#include "chrome/browser/extensions/extension_service.h"
#endif
#include "chrome/browser/profiles/profile.h"
#if defined(ENABLE_WEBUI)        
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#endif
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace tab_util {

content::WebContents* GetWebContentsByID(int render_process_id,
                                         int render_view_id) {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  return WebContents::FromRenderViewHost(render_view_host);
}

SiteInstance* GetSiteInstanceForNewTab(Profile* profile,
                                       const GURL& url) {
  // If |url| is a WebUI or extension, we set the SiteInstance up front so that
  // we don't end up with an extra process swap on the first navigation.
#if defined(ENABLE_EXTENSIONS_ALL)  
  ExtensionService* service = profile->GetExtensionService();
#endif
#if defined(ENABLE_WEBUI)        
  if (ChromeWebUIControllerFactory::GetInstance()->UseWebUIForURL(
#if defined(ENABLE_EXTENSIONS_ALL)  
          profile, url) ||
      (service &&
       service->extensions()->GetHostedAppByURL(url))) {
#else
          profile, url)) {
#endif
    return SiteInstance::CreateForURL(profile, url);
  }
#endif //ENABLE_WEBUI

  // We used to share the SiteInstance for same-site links opened in new tabs,
  // to leverage the in-memory cache and reduce process creation.  It now
  // appears that it is more useful to have such links open in a new process,
  // so we create new tabs in a new BrowsingInstance.
  return NULL;
}

}  // namespace tab_util
