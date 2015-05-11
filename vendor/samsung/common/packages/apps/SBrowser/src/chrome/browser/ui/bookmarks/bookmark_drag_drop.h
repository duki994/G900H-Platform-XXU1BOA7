// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_

#include <vector>

#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/native_widget_types.h"

class BookmarkNode;
struct BookmarkNodeData;
class Profile;

namespace chrome {

// Starts the process of dragging a folder of bookmarks.
void DragBookmarks(Profile* profile,
                   const std::vector<const BookmarkNode*>& nodes,
                   gfx::NativeView view,
                   ui::DragDropTypes::DragEventSource source);

// Drops the bookmark nodes that are in |data| onto |parent_node| at |index|.
// Returns the drop type used.
int DropBookmarks(Profile* profile,
                  const BookmarkNodeData& data,
                  const BookmarkNode* parent_node,
                  int index);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_DRAG_DROP_H_
