// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
#define UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_

#include <string>
#include <vector>

#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/app_list_item_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace app_list {

class AppListItemList;

typedef std::vector<gfx::Rect> Rects;

// AppListFolderItem implements the model/controller for folders.
class APP_LIST_EXPORT AppListFolderItem : public AppListItem,
                                          public AppListItemListObserver,
                                          public AppListItemObserver {
 public:
  static const char kItemType[];

  explicit AppListFolderItem(const std::string& id);
  virtual ~AppListFolderItem();

  // Updates the folder's icon.
  void UpdateIcon();

  // Returns the icon of one of the top items with |item_index|.
  const gfx::ImageSkia& GetTopIcon(size_t item_index);

  // Returns the target icon bounds for |item| to fly back to its parent folder
  // icon in animation UI. If |item| is one of the top item icon, this will
  // match its corresponding top item icon in the folder icon. Otherwise,
  // the target icon bounds is centered at the |folder_icon_bounds| with
  // the same size of the top item icon.
  // The Rect returned is in the same coordinates of |folder_icon_bounds|.
  gfx::Rect GetTargetIconRectInFolderForItem(
      AppListItem* item, const gfx::Rect& folder_icon_bounds);

  AppListItemList* item_list() { return item_list_.get(); }
  const AppListItemList* item_list() const { return item_list_.get(); }

  // AppListItem
  virtual void Activate(int event_flags) OVERRIDE;
  virtual const char* GetItemType() const OVERRIDE;
  virtual ui::MenuModel* GetContextMenuModel() OVERRIDE;
  virtual AppListItem* FindChildItem(const std::string& id) OVERRIDE;
  virtual size_t ChildItemCount() const OVERRIDE;
  virtual bool CompareForTest(const AppListItem* other) const OVERRIDE;

  // Calculates the top item icons' bounds inside |folder_icon_bounds|.
  // Returns the bounds of top item icons in sequence of top left, top right,
  // bottom left, bottom right.
  static Rects GetTopIconsBounds(const gfx::Rect& folder_icon_bounds);

  // Returns an id for a new folder.
  static std::string GenerateId();

 private:
  // AppListItemObserver
  virtual void ItemIconChanged() OVERRIDE;
  virtual void ItemTitleChanged() OVERRIDE;
  virtual void ItemHighlightedChanged() OVERRIDE;
  virtual void ItemIsInstallingChanged() OVERRIDE;
  virtual void ItemPercentDownloadedChanged() OVERRIDE;

  // AppListItemListObserver
  virtual void OnListItemAdded(size_t index, AppListItem* item) OVERRIDE;
  virtual void OnListItemRemoved(size_t index,
                                 AppListItem* item) OVERRIDE;;
  virtual void OnListItemMoved(size_t from_index,
                               size_t to_index,
                               AppListItem* item) OVERRIDE;

  void UpdateTopItems();

  scoped_ptr<AppListItemList> item_list_;
  // Top items for generating folder icon.
  std::vector<AppListItem*> top_items_;

  DISALLOW_COPY_AND_ASSIGN(AppListFolderItem);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_FOLDER_ITEM_H_
