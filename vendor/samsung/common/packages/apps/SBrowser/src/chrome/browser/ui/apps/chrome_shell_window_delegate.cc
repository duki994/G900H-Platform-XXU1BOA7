// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_shell_window_delegate.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

#if defined(USE_ASH)
#include "ash/shelf/shelf_constants.h"
#endif

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)

namespace {

bool disable_external_open_for_testing_ = false;

// Opens a URL with Chromium (not external browser) with the right profile.
content::WebContents* OpenURLFromTabInternal(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // Force all links to open in a new tab, even if they were trying to open a
  // window.
  chrome::NavigateParams new_tab_params(
      static_cast<Browser*>(NULL), params.url, params.transition);
  new_tab_params.disposition = params.disposition == NEW_BACKGROUND_TAB
                                   ? params.disposition
                                   : NEW_FOREGROUND_TAB;
  new_tab_params.initiating_profile = Profile::FromBrowserContext(context);
  chrome::Navigate(&new_tab_params);

  return new_tab_params.target_contents;
}

// Helper class that opens a URL based on if this browser instance is the
// default system browser. If it is the default, open the URL directly instead
// of asking the system to open it.
class OpenURLFromTabBasedOnBrowserDefault
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  OpenURLFromTabBasedOnBrowserDefault(content::WebContents* source,
                                      const content::OpenURLParams& params)
      : source_(source), params_(params) {}

  // Opens a URL when called with the result of if this is the default system
  // browser or not.
  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE {
    Profile* profile =
        Profile::FromBrowserContext(source_->GetBrowserContext());
    DCHECK(profile);
    if (!profile)
      return;
    switch (state) {
      case ShellIntegration::STATE_PROCESSING:
        break;
      case ShellIntegration::STATE_IS_DEFAULT:
        OpenURLFromTabInternal(profile, source_, params_);
        break;
      case ShellIntegration::STATE_NOT_DEFAULT:
      case ShellIntegration::STATE_UNKNOWN:
        platform_util::OpenExternal(profile, params_.url);
        break;
    }
  }

  virtual bool IsOwnedByWorker() OVERRIDE { return true; }

 private:
  content::WebContents* source_;
  const content::OpenURLParams params_;
};

}  // namespace

ShellWindowLinkDelegate::ShellWindowLinkDelegate() {}

ShellWindowLinkDelegate::~ShellWindowLinkDelegate() {}

// TODO(rockot): Add a test that exercises this code. See
// http://crbug.com/254260.
content::WebContents* ShellWindowLinkDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (source) {
    scoped_refptr<ShellIntegration::DefaultWebClientWorker>
        check_if_default_browser_worker =
            new ShellIntegration::DefaultBrowserWorker(
                new OpenURLFromTabBasedOnBrowserDefault(source, params));
    // Object lifetime notes: The OpenURLFromTabBasedOnBrowserDefault is owned
    // by check_if_default_browser_worker. StartCheckIsDefault() takes lifetime
    // ownership of check_if_default_browser_worker and will clean up after
    // the asynchronous tasks.
    check_if_default_browser_worker->StartCheckIsDefault();
  }
  return NULL;
}

ChromeShellWindowDelegate::ChromeShellWindowDelegate() {}

ChromeShellWindowDelegate::~ChromeShellWindowDelegate() {}

void ChromeShellWindowDelegate::DisableExternalOpenForTesting() {
  disable_external_open_for_testing_ = true;
}

void ChromeShellWindowDelegate::InitWebContents(
    content::WebContents* web_contents) {
  FaviconTabHelper::CreateForWebContents(web_contents);

#if defined(ENABLE_PRINTING)
#if defined(ENABLE_FULL_PRINTING)
  printing::PrintViewManager::CreateForWebContents(web_contents);
  printing::PrintPreviewMessageHandler::CreateForWebContents(web_contents);
#else
  printing::PrintViewManagerBasic::CreateForWebContents(web_contents);
#endif  // defined(ENABLE_FULL_PRINTING)
#endif  // defined(ENABLE_PRINTING)
}

apps::NativeAppWindow* ChromeShellWindowDelegate::CreateNativeAppWindow(
    apps::AppWindow* window,
    const apps::AppWindow::CreateParams& params) {
  return CreateNativeAppWindowImpl(window, params);
}

content::WebContents* ChromeShellWindowDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return OpenURLFromTabInternal(context, source, params);
}

void ChromeShellWindowDelegate::AddNewContents(
    content::BrowserContext* context,
    content::WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  if (!disable_external_open_for_testing_) {
    if (!shell_window_link_delegate_.get())
      shell_window_link_delegate_.reset(new ShellWindowLinkDelegate());
    new_contents->SetDelegate(shell_window_link_delegate_.get());
    return;
  }
  chrome::ScopedTabbedBrowserDisplayer displayer(
      Profile::FromBrowserContext(context), chrome::GetActiveDesktop());
  // Force all links to open in a new tab, even if they were trying to open a
  // new window.
  disposition =
      disposition == NEW_BACKGROUND_TAB ? disposition : NEW_FOREGROUND_TAB;
  chrome::AddWebContents(displayer.browser(), NULL, new_contents, disposition,
                         initial_pos, user_gesture, was_blocked);
}

content::ColorChooser* ChromeShellWindowDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void ChromeShellWindowDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

void ChromeShellWindowDelegate::RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

int ChromeShellWindowDelegate::PreferredIconSize() {
#if defined(USE_ASH)
  return ash::kShelfPreferredSize;
#else
  return extension_misc::EXTENSION_ICON_SMALL;
#endif
}

void ChromeShellWindowDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  // RenderViewHost may be NULL during shutdown.
  content::RenderViewHost* host = web_contents->GetRenderViewHost();
  if (host) {
    host->Send(new ChromeViewMsg_SetVisuallyDeemphasized(
        host->GetRoutingID(), blocked));
  }
}

bool ChromeShellWindowDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetView()->GetNativeView());
}
