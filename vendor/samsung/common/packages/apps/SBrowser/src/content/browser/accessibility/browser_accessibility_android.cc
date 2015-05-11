// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/common/accessibility_messages.h"

namespace {

// These are enums from android.text.InputType in Java:
enum {
  ANDROID_TEXT_INPUTTYPE_TYPE_NULL = 0,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME = 0x4,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE = 0x14,
  ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME = 0x24,
  ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER = 0x2,
  ANDROID_TEXT_INPUTTYPE_TYPE_PHONE = 0x3,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT = 0x1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI = 0x11,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EDIT_TEXT = 0xa1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL = 0xd1,
  ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD = 0xe1
};

// These are enums from android.view.View in Java:
enum {
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE = 0,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE = 1,
  ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE = 2
};

// These are enums from
// android.view.accessibility.AccessibilityNodeInfo.RangeInfo in Java:
enum {
  ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT = 1
};

}  // namespace

namespace content {

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAndroid();
}

BrowserAccessibilityAndroid::BrowserAccessibilityAndroid() {
  first_time_ = true;
}

bool BrowserAccessibilityAndroid::IsNative() const {
  return true;
}

bool BrowserAccessibilityAndroid::PlatformIsLeaf() const {
  if (child_count() == 0)
    return true;

  // Iframes are always allowed to contain children.
  if (IsIframe() ||
      role() == ui::AX_ROLE_ROOT_WEB_AREA ||
      role() == ui::AX_ROLE_WEB_AREA) {
    return false;
  }

  // If it has a focusable child, we definitely can't leave out children.
  if (HasFocusableChild())
    return false;

  // Headings with text can drop their children.
  base::string16 name = GetText();
  if (role() == ui::AX_ROLE_HEADING && !name.empty())
    return true;

  // Focusable nodes with text can drop their children.
  if (HasState(ui::AX_STATE_FOCUSABLE) && !name.empty())
    return true;

  // Nodes with only static text as children can drop their children.
  if (HasOnlyStaticTextChildren())
    return true;

  return BrowserAccessibility::PlatformIsLeaf();
}

bool BrowserAccessibilityAndroid::IsCheckable() const {
  bool checkable = false;
  bool is_aria_pressed_defined;
  bool is_mixed;
  GetAriaTristate("aria-pressed", &is_aria_pressed_defined, &is_mixed);
  if (role() == ui::AX_ROLE_CHECK_BOX ||
      role() == ui::AX_ROLE_RADIO_BUTTON ||
      is_aria_pressed_defined) {
    checkable = true;
  }
  if (HasState(ui::AX_STATE_CHECKED))
    checkable = true;
  return checkable;
}

bool BrowserAccessibilityAndroid::IsChecked() const {
  return HasState(ui::AX_STATE_CHECKED);
}

bool BrowserAccessibilityAndroid::IsClickable() const {
  return (PlatformIsLeaf() && !GetText().empty());
}

bool BrowserAccessibilityAndroid::IsCollection() const {
  return (role() == ui::AX_ROLE_GRID ||
          role() == ui::AX_ROLE_LIST ||
          role() == ui::AX_ROLE_LIST_BOX ||
          role() == ui::AX_ROLE_TABLE ||
          role() == ui::AX_ROLE_TREE);
}

bool BrowserAccessibilityAndroid::IsCollectionItem() const {
  return (role() == ui::AX_ROLE_CELL ||
          role() == ui::AX_ROLE_COLUMN_HEADER ||
          role() == ui::AX_ROLE_DESCRIPTION_LIST_TERM ||
          role() == ui::AX_ROLE_LIST_BOX_OPTION ||
          role() == ui::AX_ROLE_LIST_ITEM ||
          role() == ui::AX_ROLE_ROW_HEADER ||
          role() == ui::AX_ROLE_TREE_ITEM);
}

bool BrowserAccessibilityAndroid::IsContentInvalid() const {
  std::string invalid;
  return GetHtmlAttribute("aria-invalid", &invalid);
}

bool BrowserAccessibilityAndroid::IsDismissable() const {
  return false;  // No concept of "dismissable" on the web currently.
}

bool BrowserAccessibilityAndroid::IsEnabled() const {
  return HasState(ui::AX_STATE_ENABLED);
}

bool BrowserAccessibilityAndroid::IsFocusable() const {
  bool focusable = HasState(ui::AX_STATE_FOCUSABLE);
  if (IsIframe() ||
      role() == ui::AX_ROLE_WEB_AREA) {
    focusable = false;
  }
  return focusable;
}

bool BrowserAccessibilityAndroid::IsFocused() const {
  return manager()->GetFocus(manager()->GetRoot()) == this;
}

bool BrowserAccessibilityAndroid::IsHeading() const {
  return (role() == ui::AX_ROLE_COLUMN_HEADER ||
          role() == ui::AX_ROLE_HEADING ||
          role() == ui::AX_ROLE_ROW_HEADER);
}

bool BrowserAccessibilityAndroid::IsHierarchical() const {
  return (role() == ui::AX_ROLE_LIST ||
          role() == ui::AX_ROLE_TREE);
}

bool BrowserAccessibilityAndroid::IsMultiLine() const {
  return role() == ui::AX_ROLE_TEXT_AREA;
}

bool BrowserAccessibilityAndroid::IsPassword() const {
  return HasState(ui::AX_STATE_PROTECTED);
}

bool BrowserAccessibilityAndroid::IsRangeType() const {
  return (role() == ui::AX_ROLE_PROGRESS_INDICATOR ||
          role() == ui::AX_ROLE_SCROLL_BAR ||
          role() == ui::AX_ROLE_SLIDER);
}

bool BrowserAccessibilityAndroid::IsScrollable() const {
  int dummy;
  return GetIntAttribute(ui::AX_ATTR_SCROLL_X_MAX, &dummy);
}

bool BrowserAccessibilityAndroid::IsSelected() const {
  return HasState(ui::AX_STATE_SELECTED);
}

bool BrowserAccessibilityAndroid::IsVisibleToUser() const {
  return !HasState(ui::AX_STATE_INVISIBLE);
}

bool BrowserAccessibilityAndroid::CanOpenPopup() const {
  return HasState(ui::AX_STATE_HASPOPUP);
}

const char* BrowserAccessibilityAndroid::GetClassName() const {
  const char* class_name = NULL;

  switch(role()) {
    case ui::AX_ROLE_EDITABLE_TEXT:
    case ui::AX_ROLE_SPIN_BUTTON:
    case ui::AX_ROLE_TEXT_AREA:
    case ui::AX_ROLE_TEXT_FIELD:
      class_name = "android.widget.EditText";
      break;
    case ui::AX_ROLE_SLIDER:
      class_name = "android.widget.SeekBar";
      break;
    case ui::AX_ROLE_COMBO_BOX:
      class_name = "android.widget.Spinner";
      break;
    case ui::AX_ROLE_BUTTON:
    case ui::AX_ROLE_MENU_BUTTON:
    case ui::AX_ROLE_POP_UP_BUTTON:
      class_name = "android.widget.Button";
      break;
    case ui::AX_ROLE_CHECK_BOX:
      class_name = "android.widget.CheckBox";
      break;
    case ui::AX_ROLE_RADIO_BUTTON:
      class_name = "android.widget.RadioButton";
      break;
    case ui::AX_ROLE_TOGGLE_BUTTON:
      class_name = "android.widget.ToggleButton";
      break;
    case ui::AX_ROLE_CANVAS:
    case ui::AX_ROLE_IMAGE:
      class_name = "android.widget.Image";
      break;
    case ui::AX_ROLE_PROGRESS_INDICATOR:
      class_name = "android.widget.ProgressBar";
      break;
    case ui::AX_ROLE_TAB_LIST:
      class_name = "android.widget.TabWidget";
      break;
    case ui::AX_ROLE_GRID:
    case ui::AX_ROLE_TABLE:
      class_name = "android.widget.GridView";
      break;
    case ui::AX_ROLE_LIST:
    case ui::AX_ROLE_LIST_BOX:
      class_name = "android.widget.ListView";
      break;
    case ui::AX_ROLE_DIALOG:
      class_name = "android.app.Dialog";
      break;
    default:
      class_name = "android.view.View";
      break;
  }

  return class_name;
}

base::string16 BrowserAccessibilityAndroid::GetText() const {
  if (IsIframe() ||
      role() == ui::AX_ROLE_WEB_AREA) {
    return base::string16();
  }

  base::string16 description = GetString16Attribute(
      ui::AX_ATTR_DESCRIPTION);
  base::string16 text;
  if (!name().empty())
    text = base::UTF8ToUTF16(name());
  else if (!description.empty())
    text = description;
  else if (!value().empty())
    text = base::UTF8ToUTF16(value());

  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  if (text.empty() && HasOnlyStaticTextChildren()) {
    for (uint32 i = 0; i < child_count(); i++) {
      BrowserAccessibility* child = children()[i];
      text += static_cast<BrowserAccessibilityAndroid*>(child)->GetText();
    }
  }

  switch(role()) {
    case ui::AX_ROLE_IMAGE_MAP_LINK:
    case ui::AX_ROLE_LINK:
      if (!text.empty())
        text += base::ASCIIToUTF16(" ");
      text += base::ASCIIToUTF16("Link");
      break;
    case ui::AX_ROLE_HEADING:
      // Only append "heading" if this node already has text.
      if (!text.empty())
        text += base::ASCIIToUTF16(" Heading");
      break;
  }

  return text;
}

int BrowserAccessibilityAndroid::GetItemIndex() const {
  int index = 0;
  switch(role()) {
    case ui::AX_ROLE_LIST_ITEM:
    case ui::AX_ROLE_LIST_BOX_OPTION:
    case ui::AX_ROLE_TREE_ITEM:
      index = index_in_parent();
      break;
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_PROGRESS_INDICATOR: {
      float value_for_range;
      if (GetFloatAttribute(
              ui::AX_ATTR_VALUE_FOR_RANGE, &value_for_range)) {
        index = static_cast<int>(value_for_range);
      }
      break;
    }
  }
  return index;
}

int BrowserAccessibilityAndroid::GetItemCount() const {
  int count = 0;
  switch(role()) {
    case ui::AX_ROLE_LIST:
    case ui::AX_ROLE_LIST_BOX:
      count = PlatformChildCount();
      break;
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_PROGRESS_INDICATOR: {
      float max_value_for_range;
      if (GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE,
                            &max_value_for_range)) {
        count = static_cast<int>(max_value_for_range);
      }
      break;
    }
  }
  return count;
}

int BrowserAccessibilityAndroid::GetScrollX() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_X, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetScrollY() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_Y, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMaxScrollX() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_X_MAX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMaxScrollY() const {
  int value = 0;
  GetIntAttribute(ui::AX_ATTR_SCROLL_Y_MAX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetTextChangeFromIndex() const {
  size_t index = 0;
  while (index < old_value_.length() &&
         index < new_value_.length() &&
         old_value_[index] == new_value_[index]) {
    index++;
  }
  return index;
}

int BrowserAccessibilityAndroid::GetTextChangeAddedCount() const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (new_len - left - right);
}

int BrowserAccessibilityAndroid::GetTextChangeRemovedCount() const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (old_len - left - right);
}

base::string16 BrowserAccessibilityAndroid::GetTextChangeBeforeText() const {
  return old_value_;
}

int BrowserAccessibilityAndroid::GetSelectionStart() const {
  int sel_start = 0;
  GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START, &sel_start);
  return sel_start;
}

int BrowserAccessibilityAndroid::GetSelectionEnd() const {
  int sel_end = 0;
  GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END, &sel_end);
  return sel_end;
}

int BrowserAccessibilityAndroid::GetEditableTextLength() const {
  return value().length();
}

int BrowserAccessibilityAndroid::AndroidInputType() const {
  std::string html_tag = GetStringAttribute(
      ui::AX_ATTR_HTML_TAG);
  if (html_tag != "input")
    return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;

  std::string type;
  if (!GetHtmlAttribute("type", &type))
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;

  if (type == "" || type == "text" || type == "search")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT;
  else if (type == "date")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  else if (type == "datetime" || type == "datetime-local")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;
  else if (type == "email")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_EMAIL;
  else if (type == "month")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_DATE;
  else if (type == "number")
    return ANDROID_TEXT_INPUTTYPE_TYPE_NUMBER;
  else if (type == "password")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_WEB_PASSWORD;
  else if (type == "tel")
    return ANDROID_TEXT_INPUTTYPE_TYPE_PHONE;
  else if (type == "time")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME_TIME;
  else if (type == "url")
    return ANDROID_TEXT_INPUTTYPE_TYPE_TEXT_URI;
  else if (type == "week")
    return ANDROID_TEXT_INPUTTYPE_TYPE_DATETIME;

  return ANDROID_TEXT_INPUTTYPE_TYPE_NULL;
}

int BrowserAccessibilityAndroid::AndroidLiveRegionType() const {
  std::string live = GetStringAttribute(
      ui::AX_ATTR_LIVE_STATUS);
  if (live == "polite")
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_POLITE;
  else if (live == "assertive")
    return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_ASSERTIVE;
  return ANDROID_VIEW_VIEW_ACCESSIBILITY_LIVE_REGION_NONE;
}

int BrowserAccessibilityAndroid::AndroidRangeType() const {
  return ANDROID_VIEW_ACCESSIBILITY_RANGE_TYPE_FLOAT;
}

int BrowserAccessibilityAndroid::RowCount() const {
  if (role() == ui::AX_ROLE_GRID ||
      role() == ui::AX_ROLE_TABLE) {
    return CountChildrenWithRole(ui::AX_ROLE_ROW);
  }

  if (role() == ui::AX_ROLE_LIST ||
      role() == ui::AX_ROLE_LIST_BOX ||
      role() == ui::AX_ROLE_TREE) {
    return PlatformChildCount();
  }

  return 0;
}

int BrowserAccessibilityAndroid::ColumnCount() const {
  if (role() == ui::AX_ROLE_GRID ||
      role() == ui::AX_ROLE_TABLE) {
    return CountChildrenWithRole(ui::AX_ROLE_COLUMN);
  }
  return 0;
}

int BrowserAccessibilityAndroid::RowIndex() const {
  if (role() == ui::AX_ROLE_LIST_ITEM ||
      role() == ui::AX_ROLE_LIST_BOX_OPTION ||
      role() == ui::AX_ROLE_TREE_ITEM) {
    return index_in_parent();
  }

  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_INDEX);
}

int BrowserAccessibilityAndroid::RowSpan() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_ROW_SPAN);
}

int BrowserAccessibilityAndroid::ColumnIndex() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_INDEX);
}

int BrowserAccessibilityAndroid::ColumnSpan() const {
  return GetIntAttribute(ui::AX_ATTR_TABLE_CELL_COLUMN_SPAN);
}

float BrowserAccessibilityAndroid::RangeMin() const {
  return GetFloatAttribute(ui::AX_ATTR_MIN_VALUE_FOR_RANGE);
}

float BrowserAccessibilityAndroid::RangeMax() const {
  return GetFloatAttribute(ui::AX_ATTR_MAX_VALUE_FOR_RANGE);
}

float BrowserAccessibilityAndroid::RangeCurrentValue() const {
  return GetFloatAttribute(ui::AX_ATTR_VALUE_FOR_RANGE);
}

bool BrowserAccessibilityAndroid::HasFocusableChild() const {
  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  for (uint32 i = 0; i < child_count(); i++) {
    BrowserAccessibility* child = children()[i];
    if (child->HasState(ui::AX_STATE_FOCUSABLE))
      return true;
    if (static_cast<BrowserAccessibilityAndroid*>(child)->HasFocusableChild())
      return true;
  }
  return false;
}

bool BrowserAccessibilityAndroid::HasOnlyStaticTextChildren() const {
  // This is called from PlatformIsLeaf, so don't call PlatformChildCount
  // from within this!
  for (uint32 i = 0; i < child_count(); i++) {
    BrowserAccessibility* child = children()[i];
    if (child->role() != ui::AX_ROLE_STATIC_TEXT)
      return false;
  }
  return true;
}

bool BrowserAccessibilityAndroid::IsIframe() const {
  base::string16 html_tag = GetString16Attribute(
      ui::AX_ATTR_HTML_TAG);
  return html_tag == base::ASCIIToUTF16("iframe");
}

void BrowserAccessibilityAndroid::PostInitialize() {
  BrowserAccessibility::PostInitialize();

  if (IsEditableText()) {
    if (base::UTF8ToUTF16(value()) != new_value_) {
      old_value_ = new_value_;
      new_value_ = base::UTF8ToUTF16(value());
    }
  }

  if (role() == ui::AX_ROLE_ALERT && first_time_)
    manager()->NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, this);

  base::string16 live;
  if (GetString16Attribute(
      ui::AX_ATTR_CONTAINER_LIVE_STATUS, &live)) {
    NotifyLiveRegionUpdate(live);
  }

  first_time_ = false;
}

void BrowserAccessibilityAndroid::NotifyLiveRegionUpdate(
    base::string16& aria_live) {
  if (!EqualsASCII(aria_live, aria_strings::kAriaLivePolite) &&
      !EqualsASCII(aria_live, aria_strings::kAriaLiveAssertive))
    return;

  base::string16 text = GetText();
  if (cached_text_ != text) {
    if (!text.empty()) {
      manager()->NotifyAccessibilityEvent(ui::AX_EVENT_SHOW,
                                         this);
    }
    cached_text_ = text;
  }
}

int BrowserAccessibilityAndroid::CountChildrenWithRole(ui::AXRole role) const {
  int count = 0;
  for (uint32 i = 0; i < PlatformChildCount(); i++) {
    if (PlatformGetChild(i)->role() == role)
      count++;
  }
  return count;
}

}  // namespace content
