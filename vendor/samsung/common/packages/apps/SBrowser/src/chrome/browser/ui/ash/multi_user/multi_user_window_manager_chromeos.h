// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CHROMEOS_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CHROMEOS_H_

#include <map>
#include <string>

#include "ash/session_state_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/aura/window_observer.h"
#include "ui/views/corewm/transient_window_observer.h"

class Browser;
class MultiUserNotificationBlockerChromeOS;
class MultiUserNotificationBlockerChromeOSTest;
class Profile;

namespace aura {
class Window;
}  // namespace aura

namespace ash {
namespace test {
class MultiUserWindowManagerChromeOSTest;
}  // namespace test
}  // namespace ash

namespace chrome {

class AppObserver;

// This ChromeOS implementation of the MultiUserWindowManager interface is
// detecting app and browser creations, tagging their windows automatically and
// using (currently) show and hide to make the owned windows visible - or not.
// If it becomes necessary, the function |SetWindowVisibility| can be
// overwritten to match new ways of doing this.
// Note:
// - aura::Window::Hide() is currently hiding the window and all owned transient
//   children. However aura::Window::Show() is only showing the window itself.
//   To address that, all transient children (and their children) are remembered
//   in |transient_window_to_visibility_| and monitored to keep track of the
//   visibility changes from the owning user. This way the visibility can be
//   changed back to its requested state upon showing by us - or when the window
//   gets detached from its current owning parent.
class MultiUserWindowManagerChromeOS
    : public MultiUserWindowManager,
      public ash::SessionStateObserver,
      public aura::WindowObserver,
      public content::NotificationObserver,
      public views::corewm::TransientWindowObserver {
 public:
  // Create the manager and use |active_user_id| as the active user.
  explicit MultiUserWindowManagerChromeOS(const std::string& active_user_id);
  virtual ~MultiUserWindowManagerChromeOS();

  // MultiUserWindowManager overrides:
  virtual void SetWindowOwner(
      aura::Window* window, const std::string& user_id) OVERRIDE;
  virtual const std::string& GetWindowOwner(aura::Window* window) OVERRIDE;
  virtual void ShowWindowForUser(
      aura::Window* window, const std::string& user_id) OVERRIDE;
  virtual bool AreWindowsSharedAmongUsers() OVERRIDE;
  virtual void GetOwnersOfVisibleWindows(
      std::set<std::string>* user_ids) OVERRIDE;
  virtual bool IsWindowOnDesktopOfUser(aura::Window* window,
                                       const std::string& user_id) OVERRIDE;
  virtual const std::string& GetUserPresentingWindow(
      aura::Window* window) OVERRIDE;
  virtual void AddUser(Profile* profile) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

  // SessionStateObserver overrides:
  virtual void ActiveUserChanged(const std::string& user_id) OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;
  virtual void OnWindowVisibilityChanging(aura::Window* window,
                                          bool visible) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;

  // TransientWindowObserver overrides:
  virtual void OnTransientChildAdded(aura::Window* window,
                                     aura::Window* transient) OVERRIDE;
  virtual void OnTransientChildRemoved(aura::Window* window,
                                       aura::Window* transient) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) OVERRIDE;

  // Disable any animations for unit tests.
  void SetAnimationsForTest(bool disable);

  // Returns true when a user switch animation is running. For unit tests.
  bool IsAnimationRunningForTest();

  // Returns the current user for unit tests.
  const std::string& GetCurrentUserForTest();

 private:
  friend class ::MultiUserNotificationBlockerChromeOSTest;
  friend class ash::test::MultiUserWindowManagerChromeOSTest;

  class WindowEntry {
   public:
    explicit WindowEntry(const std::string& user_id)
        : owner_(user_id),
          show_for_user_(user_id),
          show_(true) {}
    virtual ~WindowEntry() {}

    // Returns the owner of this window. This cannot be changed.
    const std::string& owner() const { return owner_; }

    // Returns the user for which this should be shown.
    const std::string& show_for_user() const { return show_for_user_; }

    // Returns if the window should be shown for the "show user" or not.
    bool show() const { return show_; }

    // Set the user which will display the window on the owned desktop. If
    // an empty user id gets passed the owner will be used.
    void set_show_for_user(const std::string& user_id) {
      show_for_user_ = user_id.empty() ? owner_ : user_id;
    }

    // Sets if the window gets shown for the active user or not.
    void set_show(bool show) { show_ = show; }

   private:
    // The user id of the owner of this window.
    const std::string owner_;

    // The user id of the user on which desktop the window gets shown.
    std::string show_for_user_;

    // True if the window should be visible for the user which shows the window.
    bool show_;

    DISALLOW_COPY_AND_ASSIGN(WindowEntry);
  };

  typedef std::map<aura::Window*, WindowEntry*> WindowToEntryMap;
  typedef std::map<std::string, AppObserver*> UserIDToAppWindowObserver;
  typedef std::map<aura::Window*, bool> TransientWindowToVisibility;

  // The animation step for the user change animation. First the old user gets
  // hidden and then the new one gets presented.
  enum AnimationStep {
    HIDE_OLD_USER,
    SHOW_NEW_USER
  };

  // Show a window for a user without switching the user.
  // Returns true when the window moved to a new desktop.
  bool ShowWindowForUserIntern(aura::Window* window,
                               const std::string& user_id);

  // Start the user change animation required for |animation_step|.
  // Note that a call with SHOW_NEW_USER will finalize the animation and kill
  // the timer (if there is one).
  void TransitionUser(AnimationStep animtion_step);

  // Start the user wallpaper animations.
  void TransitionWallpaper(AnimationStep animtion_step);

  // Start the user shelf animations.
  void TransitionUserShelf(AnimationStep animtion_step);

  // Add a browser window to the system so that the owner can be remembered.
  void AddBrowserWindow(Browser* browser);

  // Show / hide the given window. Note: By not doing this within the functions,
  // this allows to either switching to different ways to show/hide and / or to
  // distinguish state changes performed by this class vs. state changes
  // performed by the others. Note furthermore that system modal dialogs will
  // not get hidden. We will switch instead to the owners desktop.
  // The |animation_time_in_ms| is the time the animation should take. Set to 0
  // if it should get set instantly.
  void SetWindowVisibility(aura::Window* window,
                           bool visible,
                           int animation_time_in_ms);

  // Show the window and its transient children. However - if a transient child
  // was turned invisible by some other operation, it will stay invisible.
  // Use the given |animation_time_in_ms| for transitioning.
  void ShowWithTransientChildrenRecursive(aura::Window* window,
                                          int animation_time_in_ms);

  // Find the first owned window in the chain.
  // Returns NULL when the window itself is owned.
  aura::Window* GetOwningWindowInTransientChain(aura::Window* window);

  // A |window| and its children were attached as transient children to an
  // |owning_parent| and need to be registered. Note that the |owning_parent|
  // itself will not be registered, but its children will.
  void AddTransientOwnerRecursive(aura::Window* window,
                                  aura::Window* owning_parent);

  // A window and its children were removed from its parent and can be
  // unregistered.
  void RemoveTransientOwnerRecursive(aura::Window* window);

  // Animate a |window| to be |visible| in |animation_time_in_ms|.
  void SetWindowVisible(aura::Window* window,
                        bool visible,
                        int aimation_time_in_ms);

  // A lookup to see to which user the given window belongs to, where and if it
  // should get shown.
  WindowToEntryMap window_to_entry_;

  // A list of all known users and their app window observers.
  UserIDToAppWindowObserver user_id_to_app_observer_;

  // An observer list to be notified upon window owner changes.
  ObserverList<Observer> observers_;

  // A map which remembers for owned transient windows their own visibility.
  TransientWindowToVisibility transient_window_to_visibility_;

  // The currently selected active user. It is used to find the proper
  // visibility state in various cases. The state is stored here instead of
  // being read from the user manager to be in sync while a switch occurs.
  std::string current_user_id_;

  // The blocker which controls the desktop notification visibility based on the
  // current multi-user status.
  scoped_ptr<MultiUserNotificationBlockerChromeOS> notification_blocker_;

  // The notification registrar to track the creation of browser windows.
  content::NotificationRegistrar registrar_;

  // Suppress changes to the visibility flag while we are changing it ourselves.
  bool suppress_visibility_changes_;

  // Caching the current multi profile mode since the detection which mode is
  // used is quite expensive.
  static MultiProfileMode multi_user_mode_;

  // A timer which watches to executes the second part of a "user changed"
  // animation. Note that this timer exists only during such an animation.
  scoped_ptr<base::Timer> user_changed_animation_timer_;

  // If true, all animations will be suppressed.
  bool animations_disabled_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerChromeOS);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CHROMEOS_H_
