

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_HISTORY_HISTORY_MODEL_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_HISTORY_HISTORY_MODEL_H_

#include <string>
#include "base/android/jni_helper.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "base/task/cancelable_task_tracker.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"

class HistoryModel : public content::WebUIMessageHandler,
                     public content::NotificationObserver,
                     public BrowsingDataRemover::Observer {
 public:
  HistoryModel(JNIEnv* env, jobject obj);

  // Registers the HistoryModel native method.
  static bool RegisterHistoryModel(JNIEnv* env);

  //Register Messages for runtime updation
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "GetAllHistory" message.
  void GetAllHistory(JNIEnv* env, jobject obj, jint offset, jint range,
                     jdouble end_time, jint max_count);

  // Callback for the "ClearAllHistory" message.
  void ClearAllHistory(JNIEnv* env, jobject obj);

  // Callback for the "SearchHistory" message.
  void SearchHistory(JNIEnv* env, jobject obj, jstring value);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Callback for the "Destroy" message.
  void Destroy(JNIEnv* env, jobject obj);

  void ClearHistoryURLsDone();

  void OnBrowsingDataRemoverDone();

 private:
  virtual ~HistoryModel();

  // The range for which to return results:
  // - ALLTIME: allows access to all the results in a paginated way.
  // - WEEK: the last 7 days.
  // - MONTH: the last calendar month.
  enum Range {
    ALL_TIME = 0,
    WEEK = 1,
    MONTH = 2
  };

  //Query History results
  void QueryHistory(base::string16 search_text, const history::QueryOptions& options);

   // Sets the query options for a week-wide query, |offset| weeks ago.
  void SetQueryTimeInWeeks(int offset, history::QueryOptions* options);

  // Sets the query options for a monthly query, |offset| months ago.
  void SetQueryTimeInMonths(int offset, history::QueryOptions* options);

  // Callback from the history system when the history list is available.
  void QueryComplete(HistoryService::Handle request_handle,
                     history::QueryResults* results);

  void GetMostVisited();

  void NotifyHistoryCommitted();

  // Figure out the query options for a month-wide query.
  history::QueryOptions CreateMonthQueryOptions(int month);

  content::NotificationRegistrar registrar_;

  // Our consumer for search requests to the history service.
  CancelableRequestConsumerT<int, 0> cancelable_search_consumer_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  // The list of URLs that are in the process of being deleted.
  std::set<GURL> urls_to_be_deleted_;

  JavaObjectWeakGlobalRef weak_java_histroy_model;

  // Current search text.
  base::string16 search_text_;

  // no of most_visited_url required
  int result_count_;

  // most_visited_urls from no of days history
  int days_back_;
  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* remover_;

  history::MostVisitedURLList pages_;
  GURL current_url_;

  bool got_first_most_visited_request_;

};

#endif //SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_HISTORY_HISTORY_MODEL_H_

