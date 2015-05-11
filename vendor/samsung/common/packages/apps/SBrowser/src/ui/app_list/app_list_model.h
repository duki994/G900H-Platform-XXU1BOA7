// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_MODEL_H_
#define UI_APP_LIST_APP_LIST_MODEL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/base/models/list_model.h"

namespace app_list {

class AppListFolderItem;
class AppListItem;
class AppListItemList;
class AppListModelObserver;
class SearchBoxModel;
class SearchResult;

// Master model of app list that consists of three sub models: AppListItemList,
// SearchBoxModel and SearchResults. The AppListItemList sub model owns a list
// of AppListItems and is displayed in the grid view. SearchBoxModel is
// the model for SearchBoxView. SearchResults owns a list of SearchResult.
// NOTE: Currently this class observes |item_list_|. The View code may
// move entries in the item list directly (but can not add or remove them) and
// the model needs to notify its observers when this occurs.
class APP_LIST_EXPORT AppListModel : public AppListItemListObserver {
 public:
  enum Status {
    STATUS_NORMAL,
    STATUS_SYNCING,  // Syncing apps or installing synced apps.
  };

  typedef ui::ListModel<SearchResult> SearchResults;

  AppListModel();
  virtual ~AppListModel();

  void AddObserver(AppListModelObserver* observer);
  void RemoveObserver(AppListModelObserver* observer);

  void SetStatus(Status status);

  // Finds the item matching |id|.
  AppListItem* FindItem(const std::string& id);

  // Find a folder item matching |id|.
  AppListFolderItem* FindFolderItem(const std::string& id);

  // Adds |item| to the model. The model takes ownership of |item|. Returns a
  // pointer to the item that is safe to use (e.g. after passing ownership).
  AppListItem* AddItem(scoped_ptr<AppListItem> item);

  // Adds |item| to an existing folder or creates a new folder. If |folder_id|
  // is empty, adds the item to the top level model instead. The model takes
  // ownership of |item|. Returns a pointer to the item that is safe to use.
  AppListItem* AddItemToFolder(scoped_ptr<AppListItem> item,
                               const std::string& folder_id);

  // Merges two items. If the target item is a folder, the source item is added
  // to the end of the target folder. Otherwise a new folder is created in the
  // same position as the target item with the target item as the first item in
  // the new folder and the source item as the second item. Returns the id of
  // the target folder. The source item may already be in a folder.
  // See also the comment for RemoveItemFromFolder.
  const std::string& MergeItems(const std::string& target_item_id,
                                const std::string& source_item_id);

  // Move |item| to the folder matching |folder_id| or to the top level if
  // |folder_id| is empty. |item|->position will determine where the item
  // is positioned. See also the comment for RemoveItemFromFolder.
  void MoveItemToFolder(AppListItem* item, const std::string& folder_id);

  // Move |item| to the folder matching |folder_id| or to the top level if
  // |folder_id| is empty. The item will be inserted before |position| or at
  // the end of the list if |position| is invalid. Note: |position| is copied
  // in case it refers to the containing folder which may get deleted. See also
  // the comment for RemoveItemFromFolder.
  void MoveItemToFolderAt(AppListItem* item,
                          const std::string& folder_id,
                          syncer::StringOrdinal position);

  // Sets the position of |item| either in |item_list_| or the folder specified
  // by |item|->folder_id().
  void SetItemPosition(AppListItem* item,
                       const syncer::StringOrdinal& new_position);

  // Deletes the item matching |id| from |item_list_| or from its folder.
  void DeleteItem(const std::string& id);

  AppListItemList* item_list() { return item_list_.get(); }
  SearchBoxModel* search_box() { return search_box_.get(); }
  SearchResults* results() { return results_.get(); }
  Status status() const { return status_; }

 private:
  // AppListItemListObserver
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) OVERRIDE;

  // Returns an existing folder matching |folder_id| or creates a new folder.
  AppListFolderItem* FindOrCreateFolderItem(const std::string& folder_id);

  // Adds |item_ptr| to |item_list_| and notifies observers.
  AppListItem* AddItemToItemListAndNotify(
      scoped_ptr<AppListItem> item_ptr);

  // Adds |item_ptr| to |item_list_| and notifies observers that an Update
  // occured (e.g. item moved from a folder).
  AppListItem* AddItemToItemListAndNotifyUpdate(
      scoped_ptr<AppListItem> item_ptr);

  // Adds |item_ptr| to |folder| and notifies observers.
  AppListItem* AddItemToFolderItemAndNotify(AppListFolderItem* folder,
                                            scoped_ptr<AppListItem> item_ptr);

  // Removes |item| from |item_list_| or calls RemoveItemFromFolder if
  // |item|->folder_id is set.
  scoped_ptr<AppListItem> RemoveItem(AppListItem* item);

  // Removes |item| from |folder|. If |folder| becomes empty, deletes |folder|
  // from |item_list_|. Does NOT trigger observers, calling function must do so.
  scoped_ptr<AppListItem> RemoveItemFromFolder(AppListFolderItem* folder,
                                               AppListItem* item);

  scoped_ptr<AppListItemList> item_list_;
  scoped_ptr<SearchBoxModel> search_box_;
  scoped_ptr<SearchResults> results_;

  Status status_;
  ObserverList<AppListModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_MODEL_H_
