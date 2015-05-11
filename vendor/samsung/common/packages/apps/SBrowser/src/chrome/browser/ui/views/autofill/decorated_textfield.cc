// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/decorated_textfield.h"

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/views/autofill/tooltip_icon.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace {

// Padding around icons inside DecoratedTextfields.
const int kTextfieldIconPadding = 3;

}  // namespace

namespace autofill {

// static
const char DecoratedTextfield::kViewClassName[] = "autofill/DecoratedTextfield";

DecoratedTextfield::DecoratedTextfield(
    const base::string16& default_value,
    const base::string16& placeholder,
    views::TextfieldController* controller)
    : invalid_(false),
      editable_(true) {
  UpdateBackground();
  UpdateBorder();

  set_placeholder_text(placeholder);
  SetText(default_value);
  set_controller(controller);
}

DecoratedTextfield::~DecoratedTextfield() {}

void DecoratedTextfield::SetInvalid(bool invalid) {
  if (invalid_ == invalid)
    return;

  invalid_ = invalid;
  UpdateBorder();
  SchedulePaint();
}

void DecoratedTextfield::SetEditable(bool editable) {
  if (editable_ == editable)
    return;

  editable_ = editable;
  UpdateBorder();
  UpdateBackground();
  SetEnabled(editable);
  IconChanged();
}

void DecoratedTextfield::SetIcon(const gfx::Image& icon) {
  if (!icon_view_ && icon.IsEmpty())
    return;

  if (icon_view_)
    RemoveChildView(icon_view_.get());

  if (!icon.IsEmpty()) {
    icon_view_.reset(new views::ImageView());
    icon_view_->set_owned_by_client();
    icon_view_->SetImage(icon.ToImageSkia());
    AddChildView(icon_view_.get());
  }

  IconChanged();
}

void DecoratedTextfield::SetTooltipIcon(const base::string16& text) {
  if (!icon_view_ && text.empty())
    return;

  if (icon_view_)
    RemoveChildView(icon_view_.get());

  if (!text.empty()) {
    icon_view_.reset(new TooltipIcon(text));
    AddChildView(icon_view_.get());
  }

  IconChanged();
}

base::string16 DecoratedTextfield::GetPlaceholderText() const {
  return editable_ ? views::Textfield::GetPlaceholderText() : base::string16();
}

const char* DecoratedTextfield::GetClassName() const {
  return kViewClassName;
}

views::View* DecoratedTextfield::GetEventHandlerForRect(const gfx::Rect& rect) {
  views::View* handler = views::Textfield::GetEventHandlerForRect(rect);
  if (handler->GetClassName() == TooltipIcon::kViewClassName)
    return handler;
  return this;
}

gfx::Size DecoratedTextfield::GetPreferredSize() {
  static const int height =
      views::LabelButton(NULL, base::string16()).GetPreferredSize().height();
  const gfx::Size size = views::Textfield::GetPreferredSize();
  return gfx::Size(size.width(), std::max(size.height(), height));
}

void DecoratedTextfield::Layout() {
  views::Textfield::Layout();

  if (icon_view_ && icon_view_->visible()) {
    gfx::Rect bounds = GetContentsBounds();
    gfx::Size icon_size = icon_view_->GetPreferredSize();
    int x = base::i18n::IsRTL() ?
        kTextfieldIconPadding :
        bounds.right() - icon_size.width() - kTextfieldIconPadding;
    // Vertically centered.
    int y = bounds.y() + (bounds.height() - icon_size.height()) / 2;
    icon_view_->SetBounds(x, y, icon_size.width(), icon_size.height());
  }
}

void DecoratedTextfield::UpdateBackground() {
  if (editable_)
    UseDefaultBackgroundColor();
  else
    SetBackgroundColor(SK_ColorTRANSPARENT);
  set_background(
      views::Background::CreateSolidBackground(GetBackgroundColor()));
}

void DecoratedTextfield::UpdateBorder() {
  scoped_ptr<views::FocusableBorder> border(new views::FocusableBorder());
  if (invalid_)
    border->SetColor(kWarningColor);
  else if (!editable_)
    border->SetColor(SK_ColorTRANSPARENT);
  SetBorder(border.PassAs<views::Border>());
}

void DecoratedTextfield::IconChanged() {
  // Don't show the icon if nothing else is showing.
  icon_view_->SetVisible(editable_ || !text().empty());
  Layout();
}

} // namespace autofill
