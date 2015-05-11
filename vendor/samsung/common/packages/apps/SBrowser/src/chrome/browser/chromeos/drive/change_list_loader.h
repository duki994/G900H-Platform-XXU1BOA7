// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/drive/gdata_errorcode.h"

class GURL;

namespace base {
class ScopedClosureRunner;
class SequencedTaskRunner;
class Time;
}  // namespace base

namespace google_apis {
class AboutResource;
class ResourceList;
}  // namespace google_apis

namespace drive {

class DriveServiceInterface;
class EventLogger;
class JobScheduler;
class ResourceEntry;

namespace internal {

class ChangeList;
class ChangeListLoaderObserver;
class ChangeListProcessor;
class DirectoryFetchInfo;
class ResourceMetadata;

// Delays execution of tasks as long as more than one lock is alive.
// Used to ensure that ChangeListLoader does not cause race condition by adding
// new entries created by sync tasks before they do.
// All code which may add entries found on the server to the local metadata
// should use this class.
class LoaderController {
 public:
  LoaderController();
  ~LoaderController();

  // Increments the lock count and returns an object which decrements the count
  // on its destruction.
  // While the lock count is positive, tasks will be pending.
  scoped_ptr<base::ScopedClosureRunner> GetLock();

  // Runs the task if the lock count is 0, otherwise it will be pending.
  void ScheduleRun(const base::Closure& task);

 private:
  // Decrements the lock count.
  void Unlock();

  int lock_count_;
  std::vector<base::Closure> pending_tasks_;

  base::WeakPtrFactory<LoaderController> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(LoaderController);
};

// This class is responsible to load AboutResource from the server and cache it.
class AboutResourceLoader {
 public:
  explicit AboutResourceLoader(JobScheduler* scheduler);
  ~AboutResourceLoader();

  // Returns the cached about resource.
  // NULL is returned if the cache is not available.
  const google_apis::AboutResource* cached_about_resource() const {
    return cached_about_resource_.get();
  }

  // Gets the about resource from the cache or the server. If the cache is
  // availlavle, just runs |callback| with the cached about resource. If not,
  // calls |UpdateAboutResource| passing |callback|.
  void GetAboutResource(const google_apis::AboutResourceCallback& callback);

  // Gets the about resource from the server, and caches it if successful. This
  // function calls JobScheduler::GetAboutResource internally. The cache will be
  // used in |GetAboutResource|.
  void UpdateAboutResource(
      const google_apis::AboutResourceCallback& callback);

 private:
  // Part of UpdateAboutResource().
  // This function should be called when the latest about resource is being
  // fetched from the server. The retrieved about resoure is cloned, and one is
  // cached and the other is passed to |callback|.
  void UpdateAboutResourceAfterGetAbout(
      const google_apis::AboutResourceCallback& callback,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  JobScheduler* scheduler_;
  scoped_ptr<google_apis::AboutResource> cached_about_resource_;
  base::WeakPtrFactory<AboutResourceLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(AboutResourceLoader);
};

// ChangeListLoader is used to load the change list, the full resource list,
// and directory contents, from WAPI (codename for Documents List API)
// or Google Drive API.  The class also updates the resource metadata with
// the change list loaded from the server.
//
// Note that the difference between "resource list" and "change list" is
// subtle hence the two words are often used interchangeably. To be precise,
// "resource list" refers to metadata from the server when fetching the full
// resource metadata, or fetching directory contents, whereas "change list"
// refers to metadata from the server when fetching changes (delta).
class ChangeListLoader {
 public:
  // Resource feed fetcher from the server.
  class FeedFetcher;

  ChangeListLoader(EventLogger* logger,
                   base::SequencedTaskRunner* blocking_task_runner,
                   ResourceMetadata* resource_metadata,
                   JobScheduler* scheduler,
                   DriveServiceInterface* drive_service,
                   AboutResourceLoader* about_resource_loader,
                   LoaderController* apply_task_controller);
  ~ChangeListLoader();

  // Indicates whether there is a request for full resource list or change
  // list fetching is in flight (i.e. directory contents fetching does not
  // count).
  bool IsRefreshing() const;

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Checks for updates on the server. Does nothing if the change list is now
  // being loaded or refreshed. |callback| must not be null.
  // Note: |callback| will be called if the check for updates actually
  // runs, i.e. it may NOT be called if the checking is ignored.
  void CheckForUpdates(const FileOperationCallback& callback);

  // Starts the change list loading if needed. If the locally stored metadata is
  // available, runs |callback| immediately and starts checking server for
  // updates in background. If the locally stored metadata is not available,
  // starts loading from the server, and runs |callback| to tell the result to
  // the caller when it is finished.
  //
  // The specified directory will be fetched first from the server, so the UI
  // can show the directory contents instantly before the entire change list
  // loading is complete.
  //
  // |callback| must not be null.
  void LoadDirectoryIfNeeded(const base::FilePath& directory_path,
                             const FileOperationCallback& callback);

  // Calls Load() with an empty DirectoryFetchInfo(). Only for testing purposes.
  void LoadForTesting(const FileOperationCallback& callback);

 private:
  // Part of LoadDirectoryIfNeeded().
  void LoadDirectoryIfNeededAfterGetEntry(const base::FilePath& directory_path,
                                          const FileOperationCallback& callback,
                                          bool should_try_loading_parent,
                                          const ResourceEntry* entry,
                                          FileError error);
  void LoadDirectoryIfNeededAfterLoadParent(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback,
      FileError error);

  // Starts the resource metadata loading and calls |callback| when it's
  // done. |directory_fetch_info| is used for fast fetch. If there is already
  // a loading job in-flight for |directory_fetch_info|, just append the
  // |callback| to the callback queue of the already running job.
  void Load(const DirectoryFetchInfo& directory_fetch_info,
            const FileOperationCallback& callback);
  void LoadAfterGetLargestChangestamp(
      const DirectoryFetchInfo& directory_fetch_info,
      bool is_initial_load,
      int64 local_changestamp);
  void LoadAfterGetAboutResource(
      const DirectoryFetchInfo& directory_fetch_info,
      bool is_initial_load,
      int64 local_changestamp,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);
  void LoadAfterLoadDirectory(const DirectoryFetchInfo& directory_fetch_info,
                              bool is_initial_load,
                              int64 start_changestamp,
                              FileError error);

  // Part of Load().
  // This function should be called when the change list load is complete.
  // Flushes the callbacks for change list loading and all directory loading.
  void OnChangeListLoadComplete(FileError error);

  // Part of Load().
  // This function should be called when the directory load is complete.
  // Flushes the callbacks waiting for the directory to be loaded.
  void OnDirectoryLoadComplete(const DirectoryFetchInfo& directory_fetch_info,
                               FileError error);

  // ================= Implementation for change list loading =================

  // Part of LoadFromServerIfNeeded().
  // Starts loading the change list since |start_changestamp|, or the full
  // resource list if |start_changestamp| is zero.
  void LoadChangeListFromServer(int64 start_changestamp);

  // Part of LoadChangeListFromServer().
  // Called when the entire change list is loaded.
  void LoadChangeListFromServerAfterLoadChangeList(
      scoped_ptr<google_apis::AboutResource> about_resource,
      bool is_delta_update,
      FileError error,
      ScopedVector<ChangeList> change_lists);

  // Part of LoadChangeListFromServer().
  // Called when the resource metadata is updated.
  void LoadChangeListFromServerAfterUpdate(
      ChangeListProcessor* change_list_processor,
      bool should_notify_changed_directories,
      const base::Time& start_time,
      FileError error);

  // ================= Implementation for directory loading =================
  // Loads the directory contents from server, and updates the local metadata.
  // Runs |callback| when it is finished.
  void LoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info,
                               const FileOperationCallback& callback);

  // Part of LoadDirectoryFromServer() for a normal directory.
  void LoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      FeedFetcher* fetcher,
      FileError error,
      ScopedVector<ChangeList> change_lists);

  // Part of LoadDirectoryFromServer().
  void LoadDirectoryFromServerAfterRefresh(
      const DirectoryFetchInfo& directory_fetch_info,
      const FileOperationCallback& callback,
      const base::FilePath* directory_path,
      FileError error);

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  DriveServiceInterface* drive_service_;  // Not owned.
  AboutResourceLoader* about_resource_loader_;  // Not owned.
  LoaderController* loader_controller_;  // Not owned.
  ObserverList<ChangeListLoaderObserver> observers_;
  typedef std::map<std::string, std::vector<FileOperationCallback> >
      LoadCallbackMap;
  LoadCallbackMap pending_load_callback_;
  FileOperationCallback pending_update_check_callback_;

  // Running feed fetcher.
  scoped_ptr<FeedFetcher> change_feed_fetcher_;

  // Set of the running feed fetcher for the fast fetch.
  std::set<FeedFetcher*> fast_fetch_feed_fetcher_set_;

  // True if the full resource list is loaded (i.e. the resource metadata is
  // stored locally).
  bool loaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ChangeListLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ChangeListLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_CHANGE_LIST_LOADER_H_
