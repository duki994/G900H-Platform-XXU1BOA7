

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NOTIFICATION_SBR_NOTIFICATION_UI_MANAGER_ANDROID_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NOTIFICATION_SBR_NOTIFICATION_UI_MANAGER_ANDROID_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/timer/timer.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "components/user_prefs/pref_registry_syncable.h"

#include "url/gurl.h"
#include "base/android/jni_helper.h"

class Notification;
class Profile;
class QueuedNotification;

 
class NotificationUIManagerImpl
    : public NotificationUIManager,
      public content::NotificationObserver {
 public:

  NotificationUIManagerImpl();
  virtual ~NotificationUIManagerImpl();

  
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* prefs);

  
  virtual void Add(const Notification& notification,Profile* profile) OVERRIDE;

  
  virtual bool Update(const Notification& notification,Profile* profile) OVERRIDE;

  
  virtual const Notification* FindById(const std::string& notification_id) const OVERRIDE;

  
  virtual std::set<std::string> GetAllIdsByProfileAndSourceOrigin(Profile* profile,const GURL& source) OVERRIDE;

 
  bool DoesIdExist(const std::string& notification_id);

 
  virtual bool CancelById(const std::string& notification_id) OVERRIDE;

  
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin) OVERRIDE;

 
  virtual bool CancelAllByProfile(Profile* profile) OVERRIDE;

  
  virtual void CancelAll() OVERRIDE;

  
  void GetQueuedNotificationsForTesting(
      std::vector<const Notification*>* notifications);

 protected:
  
 void CheckAndShowNotifications();

 private:
  
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  
  void ShowNotifications();

  
  bool TryReplacement(const Notification& notification);

  
  void CheckUserState();

  
  typedef std::deque<QueuedNotification*> NotificationDeque;
  NotificationDeque show_queue_;

  
  content::NotificationRegistrar registrar_;

  
  bool is_user_active_;
  base::RepeatingTimer<NotificationUIManagerImpl> user_state_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerImpl);
};

bool RegisterNotificationUIManagerImpl(JNIEnv* env);
#endif  // SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NOTIFICATION_SBR_NOTIFICATION_UI_MANAGER_ANDROID_H_

