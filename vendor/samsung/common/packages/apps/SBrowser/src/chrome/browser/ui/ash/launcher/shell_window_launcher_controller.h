// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_

#include <list>
#include <map>
#include <string>

#include "apps/app_window_registry.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/window_observer.h"

namespace apps {
class AppWindow;
}

namespace aura {

class Window;

namespace client {
class ActivationClient;
}

}

class ChromeLauncherController;
class Profile;
class ShellWindowLauncherItemController;

// ShellWindowLauncherController observes the app window registry and the
// aura window manager. It handles adding and removing launcher items from
// ChromeLauncherController.
// TODO(jamescook): Rename this to AppWindowLauncherController.
// http://crbug.com/344079
class ShellWindowLauncherController
    : public apps::AppWindowRegistry::Observer,
      public aura::WindowObserver,
      public aura::client::ActivationChangeObserver {
 public:
  explicit ShellWindowLauncherController(ChromeLauncherController* owner);
  virtual ~ShellWindowLauncherController();

  // Called by ChromeLauncherController when the active user changed and the
  // items need to be updated.
  virtual void ActiveUserChanged(const std::string& user_email) {}

  // An additional user identified by |Profile|, got added to the existing
  // session.
  virtual void AdditionalUserAddedToSession(Profile* profile);

  // Overridden from AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowIconChanged(apps::AppWindow* app_window) OVERRIDE;
  virtual void OnAppWindowRemoved(apps::AppWindow* app_window) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // Overriden from client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

 protected:
  // Registers a app window with the shelf and this object.
  void RegisterApp(apps::AppWindow* app_window);

  // Unregisters a app window with the shelf and this object.
  void UnregisterApp(aura::Window* window);

  // Check if a given window is known to the launcher controller.
  bool IsRegisteredApp(aura::Window* window);

 private:
  typedef std::map<std::string, ShellWindowLauncherItemController*>
      AppControllerMap;
  typedef std::map<aura::Window*, std::string> WindowToAppShelfIdMap;

  ShellWindowLauncherItemController* ControllerForWindow(aura::Window* window);

  ChromeLauncherController* owner_;
  // A set of unowned AppWindowRegistry pointers for loaded users.
  // Note that this will only be used with multiple users in the side by side
  // mode.
  std::set<apps::AppWindowRegistry*> registry_;
  aura::client::ActivationClient* activation_client_;

  // Map of app launcher id to controller.
  AppControllerMap app_controller_map_;

  // Allows us to get from an aura::Window to the app shelf id.
  WindowToAppShelfIdMap window_to_app_shelf_id_map_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_SHELL_WINDOW_LAUNCHER_CONTROLLER_H_
