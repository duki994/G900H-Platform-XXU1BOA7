// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PRIVATE_API_H_

#include <set>
#include <string>

#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/feedback/system_logs/about_system_logs_fetcher.h"
#include "chrome/common/extensions/api/log_private.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/net_log.h"

class Profile;

namespace extensions {

class LogPrivateAPI : public ProfileKeyedAPI,
                      public content::NotificationObserver,
                      public net::NetLog::ThreadSafeObserver {
 public:
  // Convenience method to get the LogPrivateAPI for a profile.
  static LogPrivateAPI* Get(Profile* profile);

  explicit LogPrivateAPI(Profile* profile);
  virtual ~LogPrivateAPI();

  void StartNetInternalsWatch(const std::string& extension_id);
  void StopNetInternalsWatch(const std::string& extension_id);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<LogPrivateAPI>* GetFactoryInstance();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<LogPrivateAPI>;

  // ChromeNetLog::ThreadSafeObserver implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

  void PostPendingEntries();
  void AddEntriesOnUI(scoped_ptr<base::ListValue> value);

  void MaybeStartNetInternalLogging();
  void MaybeStopNetInternalLogging();
  void StopNetInternalLogging();

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "LogPrivateAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  Profile* const profile_;
  bool logging_net_internals_;
  content::NotificationRegistrar registrar_;
  std::set<std::string> net_internal_watches_;
  scoped_ptr<base::ListValue> pending_entries_;
};

class LogPrivateGetHistoricalFunction : public AsyncExtensionFunction {
 public:
  LogPrivateGetHistoricalFunction();
  DECLARE_EXTENSION_FUNCTION("logPrivate.getHistorical",
                             LOGPRIVATE_GETHISTORICAL);

 protected:
  virtual ~LogPrivateGetHistoricalFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnSystemLogsLoaded(scoped_ptr<system_logs::SystemLogsResponse> sys_info);

  scoped_ptr<FilterHandler> filter_handler_;

  DISALLOW_COPY_AND_ASSIGN(LogPrivateGetHistoricalFunction);
};

class LogPrivateStartNetInternalsWatchFunction
    : public ChromeSyncExtensionFunction {
 public:
  LogPrivateStartNetInternalsWatchFunction();
  DECLARE_EXTENSION_FUNCTION("logPrivate.startNetInternalsWatch",
                             LOGPRIVATE_STARTNETINTERNALSWATCH);

 protected:
  virtual ~LogPrivateStartNetInternalsWatchFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogPrivateStartNetInternalsWatchFunction);
};

class LogPrivateStopNetInternalsWatchFunction
    : public ChromeSyncExtensionFunction {
 public:
  LogPrivateStopNetInternalsWatchFunction();
  DECLARE_EXTENSION_FUNCTION("logPrivate.stopNetInternalsWatch",
                             LOGPRIVATE_STOPNETINTERNALSWATCH);

 protected:
  virtual ~LogPrivateStopNetInternalsWatchFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogPrivateStopNetInternalsWatchFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOG_PRIVATE_LOG_PRIVATE_API_H_
