// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
#define APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"
#include "ui/aura/root_window_observer.h"

namespace aura {
class TestScreen;
}

namespace content {
class ShellBrowserContext;
class ShellDevToolsDelegate;
struct MainFunctionParams;
}

namespace extensions {
class ShellExtensionsBrowserClient;
class ShellExtensionSystem;
}

namespace views {
class Widget;
}

namespace net {
class NetLog;
}

namespace wm {
class WMTestHelper;
}

namespace apps {

class ShellBrowserContext;
class ShellExtensionsClient;

// Handles initialization of AppShell.
class ShellBrowserMainParts : public content::BrowserMainParts,
                              public aura::RootWindowObserver {
 public:
  explicit ShellBrowserMainParts(
      const content::MainFunctionParams& parameters);
  virtual ~ShellBrowserMainParts();

  ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

  extensions::ShellExtensionSystem* extension_system() {
    return extension_system_;
  }

  // BrowserMainParts overrides.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

  // aura::RootWindowObserver overrides:
  virtual void OnWindowTreeHostCloseRequested(const aura::RootWindow* root)
      OVERRIDE;

 private:
  // Creates the window that hosts the apps.
  void CreateRootWindow();

  // Closes and destroys the root window hosting the app.
  void DestroyRootWindow();

  // Window placement is controlled by a ViewsDelegate.
  void CreateViewsDelegate();
  void DestroyViewsDelegate();

  // Creates and initializes the ExtensionSystem.
  void CreateExtensionSystem();

  scoped_ptr<ShellBrowserContext> browser_context_;
  scoped_ptr<ShellExtensionsClient> extensions_client_;
  scoped_ptr<extensions::ShellExtensionsBrowserClient>
      extensions_browser_client_;
  scoped_ptr<net::NetLog> net_log_;

  // Enable a minimal set of views::corewm to be initialized.
  scoped_ptr<wm::WMTestHelper> wm_test_helper_;

  scoped_ptr<aura::TestScreen> test_screen_;

  scoped_ptr<views::Widget> webview_window_;

  scoped_ptr<content::ShellDevToolsDelegate> devtools_delegate_;

  // Owned by the BrowserContextKeyedService system.
  extensions::ShellExtensionSystem* extension_system_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserMainParts);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_BROWSER_MAIN_PARTS_H_
