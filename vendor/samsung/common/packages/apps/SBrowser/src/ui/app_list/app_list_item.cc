// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_item.h"

#include "base/logging.h"
#include "ui/app_list/app_list_item_observer.h"

namespace app_list {

AppListItem::AppListItem(const std::string& id)
    : id_(id),
      has_shadow_(false),
      highlighted_(false),
      is_installing_(false),
      percent_downloaded_(-1) {
}

AppListItem::~AppListItem() {
}

void AppListItem::SetIcon(const gfx::ImageSkia& icon, bool has_shadow) {
  icon_ = icon;
  has_shadow_ = has_shadow;
  FOR_EACH_OBSERVER(AppListItemObserver, observers_, ItemIconChanged());
}

void AppListItem::SetTitleAndFullName(const std::string& title,
                                      const std::string& full_name) {
  if (title_ == title && full_name_ == full_name)
    return;

  title_ = title;
  full_name_ = full_name;
  FOR_EACH_OBSERVER(AppListItemObserver, observers_, ItemTitleChanged());
}

void AppListItem::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  FOR_EACH_OBSERVER(AppListItemObserver,
                    observers_,
                    ItemHighlightedChanged());
}

void AppListItem::SetIsInstalling(bool is_installing) {
  if (is_installing_ == is_installing)
    return;

  is_installing_ = is_installing;
  FOR_EACH_OBSERVER(AppListItemObserver,
                    observers_,
                    ItemIsInstallingChanged());
}

void AppListItem::SetPercentDownloaded(int percent_downloaded) {
  if (percent_downloaded_ == percent_downloaded)
    return;

  percent_downloaded_ = percent_downloaded;
  FOR_EACH_OBSERVER(AppListItemObserver,
                    observers_,
                    ItemPercentDownloadedChanged());
}

void AppListItem::AddObserver(AppListItemObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItem::RemoveObserver(AppListItemObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListItem::Activate(int event_flags) {
}

const char* AppListItem::GetItemType() const {
  static const char* app_type = "";
  return app_type;
}

ui::MenuModel* AppListItem::GetContextMenuModel() {
  return NULL;
}

AppListItem* AppListItem::FindChildItem(const std::string& id) {
  return NULL;
}

size_t AppListItem::ChildItemCount() const {
  return 0;
}

bool AppListItem::CompareForTest(const AppListItem* other) const {
  return id_ == other->id_ &&
      folder_id_ == other->folder_id_ &&
      title_ == other->title_ &&
      GetItemType() == other->GetItemType() &&
      position_.Equals(other->position_);
}

std::string AppListItem::ToDebugString() const {
  return id_.substr(0, 8) + " '" + title_ + "'"
      + " [" + position_.ToDebugString() + "]";
}

}  // namespace app_list
