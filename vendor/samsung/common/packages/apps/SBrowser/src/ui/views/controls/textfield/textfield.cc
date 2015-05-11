// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/textfield.h"

#include <string>

#include "base/debug/trace_event.h"
#include "grit/ui_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/drag_utils.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/metrics.h"
#include "ui/views/painter.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

#if defined(OS_WIN) && defined(USE_AURA)
#include "base/win/win_util.h"
#endif

namespace {

// Default placeholder text color.
const SkColor kDefaultPlaceholderTextColor = SK_ColorLTGRAY;

void ConvertRectToScreen(const views::View* src, gfx::Rect* r) {
  DCHECK(src);

  gfx::Point new_origin = r->origin();
  views::View::ConvertPointToScreen(src, &new_origin);
  r->set_origin(new_origin);
}

}  // namespace

namespace views {

// static
const char Textfield::kViewClassName[] = "Textfield";

// static
size_t Textfield::GetCaretBlinkMs() {
  static const size_t default_value = 500;
#if defined(OS_WIN)
  static const size_t system_value = ::GetCaretBlinkTime();
  if (system_value != 0)
    return (system_value == INFINITE) ? 0 : system_value;
#endif
  return default_value;
}

Textfield::Textfield()
    : model_(new TextfieldModel(this)),
      controller_(NULL),
      read_only_(false),
      default_width_in_chars_(0),
      text_color_(SK_ColorBLACK),
      use_default_text_color_(true),
      background_color_(SK_ColorWHITE),
      use_default_background_color_(true),
      placeholder_text_color_(kDefaultPlaceholderTextColor),
      text_input_type_(ui::TEXT_INPUT_TYPE_TEXT),
      skip_input_method_cancel_composition_(false),
      cursor_visible_(false),
      drop_cursor_visible_(false),
      initiating_drag_(false),
      aggregated_clicks_(0),
      weak_ptr_factory_(this) {
  set_context_menu_controller(this);
  set_drag_controller(this);
  SetBorder(scoped_ptr<Border>(new FocusableBorder()));
  SetFocusable(true);

  if (ViewsDelegate::views_delegate) {
    password_reveal_duration_ = ViewsDelegate::views_delegate->
        GetDefaultTextfieldObscuredRevealDuration();
  }

  if (NativeViewHost::kRenderNativeControlFocus)
    focus_painter_ = Painter::CreateDashedFocusPainter();
}

Textfield::~Textfield() {}

void Textfield::SetReadOnly(bool read_only) {
  // Update read-only without changing the focusable state (or active, etc.).
  read_only_ = read_only;
  if (GetInputMethod())
    GetInputMethod()->OnTextInputTypeChanged(this);
  SetColor(GetTextColor());
  UpdateBackgroundColor();
}

void Textfield::SetTextInputType(ui::TextInputType type) {
  GetRenderText()->SetObscured(type == ui::TEXT_INPUT_TYPE_PASSWORD);
  text_input_type_ = type;
  OnCaretBoundsChanged();
  if (GetInputMethod())
    GetInputMethod()->OnTextInputTypeChanged(this);
  SchedulePaint();
}

void Textfield::SetText(const base::string16& new_text) {
  model_->SetText(new_text);
  OnCaretBoundsChanged();
  SchedulePaint();
  NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
}

void Textfield::AppendText(const base::string16& new_text) {
  if (new_text.empty())
    return;
  model_->Append(new_text);
  OnCaretBoundsChanged();
  SchedulePaint();
}

void Textfield::InsertOrReplaceText(const base::string16& new_text) {
  if (new_text.empty())
    return;
  model_->InsertText(new_text);
  OnCaretBoundsChanged();
  SchedulePaint();
}

base::i18n::TextDirection Textfield::GetTextDirection() const {
  return GetRenderText()->GetTextDirection();
}

void Textfield::SelectAll(bool reversed) {
  model_->SelectAll(reversed);
  UpdateSelectionClipboard();
  UpdateAfterChange(false, true);
}

base::string16 Textfield::GetSelectedText() const {
  return model_->GetSelectedText();
}

void Textfield::ClearSelection() {
  model_->ClearSelection();
  UpdateAfterChange(false, true);
}

bool Textfield::HasSelection() const {
  return !GetSelectedRange().is_empty();
}

SkColor Textfield::GetTextColor() const {
  if (!use_default_text_color_)
    return text_color_;

  return GetNativeTheme()->GetSystemColor(read_only() ?
      ui::NativeTheme::kColorId_TextfieldReadOnlyColor :
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

void Textfield::SetTextColor(SkColor color) {
  text_color_ = color;
  use_default_text_color_ = false;
  SetColor(color);
}

void Textfield::UseDefaultTextColor() {
  use_default_text_color_ = true;
  SetColor(GetTextColor());
}

SkColor Textfield::GetBackgroundColor() const {
  if (!use_default_background_color_)
    return background_color_;

  return GetNativeTheme()->GetSystemColor(read_only() ?
      ui::NativeTheme::kColorId_TextfieldReadOnlyBackground :
      ui::NativeTheme::kColorId_TextfieldDefaultBackground);
}

void Textfield::SetBackgroundColor(SkColor color) {
  background_color_ = color;
  use_default_background_color_ = false;
  UpdateBackgroundColor();
}

void Textfield::UseDefaultBackgroundColor() {
  use_default_background_color_ = true;
  UpdateBackgroundColor();
}

bool Textfield::GetCursorEnabled() const {
  return GetRenderText()->cursor_enabled();
}

void Textfield::SetCursorEnabled(bool enabled) {
  GetRenderText()->SetCursorEnabled(enabled);
}

const gfx::FontList& Textfield::GetFontList() const {
  return GetRenderText()->font_list();
}

void Textfield::SetFontList(const gfx::FontList& font_list) {
  GetRenderText()->SetFontList(font_list);
  OnCaretBoundsChanged();
  PreferredSizeChanged();
}

base::string16 Textfield::GetPlaceholderText() const {
  return placeholder_text_;
}

void Textfield::ShowImeIfNeeded() {
  GetInputMethod()->ShowImeIfNeeded();
}

bool Textfield::IsIMEComposing() const {
  return model_->HasCompositionText();
}

const gfx::Range& Textfield::GetSelectedRange() const {
  return GetRenderText()->selection();
}

void Textfield::SelectRange(const gfx::Range& range) {
  model_->SelectRange(range);
  UpdateAfterChange(false, true);
}

const gfx::SelectionModel& Textfield::GetSelectionModel() const {
  return GetRenderText()->selection_model();
}

void Textfield::SelectSelectionModel(const gfx::SelectionModel& sel) {
  model_->SelectSelectionModel(sel);
  UpdateAfterChange(false, true);
}

size_t Textfield::GetCursorPosition() const {
  return model_->GetCursorPosition();
}

void Textfield::SetColor(SkColor value) {
  GetRenderText()->SetColor(value);
  SchedulePaint();
}

void Textfield::ApplyColor(SkColor value, const gfx::Range& range) {
  GetRenderText()->ApplyColor(value, range);
  SchedulePaint();
}

void Textfield::SetStyle(gfx::TextStyle style, bool value) {
  GetRenderText()->SetStyle(style, value);
  SchedulePaint();
}

void Textfield::ApplyStyle(gfx::TextStyle style,
                           bool value,
                           const gfx::Range& range) {
  GetRenderText()->ApplyStyle(style, value, range);
  SchedulePaint();
}

void Textfield::ClearEditHistory() {
  model_->ClearEditHistory();
}

void Textfield::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

void Textfield::ExecuteCommand(int command_id) {
  ExecuteCommand(command_id, ui::EF_NONE);
}

void Textfield::SetFocusPainter(scoped_ptr<Painter> focus_painter) {
  focus_painter_ = focus_painter.Pass();
}

bool Textfield::HasTextBeingDragged() {
  return initiating_drag_;
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, View overrides:

int Textfield::GetBaseline() const {
  return GetInsets().top() + GetRenderText()->GetBaseline();
}

gfx::Size Textfield::GetPreferredSize() {
  const gfx::Insets& insets = GetInsets();
  return gfx::Size(GetFontList().GetExpectedTextWidth(default_width_in_chars_) +
                   insets.width(), GetFontList().GetHeight() + insets.height());
}

void Textfield::AboutToRequestFocusFromTabTraversal(bool reverse) {
  SelectAll(false);
}

bool Textfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) {
  // Skip any accelerator handling of backspace; textfields handle this key.
  // Also skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  return e.key_code() == ui::VKEY_BACK || e.IsUnicodeKeyCode();
}

void Textfield::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  PaintTextAndCursor(canvas);
  OnPaintBorder(canvas);
  if (NativeViewHost::kRenderNativeControlFocus)
    Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

bool Textfield::OnKeyPressed(const ui::KeyEvent& event) {
  bool handled = controller_ && controller_->HandleKeyEvent(this, event);
  touch_selection_controller_.reset();
  if (handled)
    return true;

  // TODO(oshima): Refactor and consolidate with ExecuteCommand.
  if (event.type() == ui::ET_KEY_PRESSED) {
    ui::KeyboardCode key_code = event.key_code();
    if (key_code == ui::VKEY_TAB || event.IsUnicodeKeyCode())
      return false;

    gfx::RenderText* render_text = GetRenderText();
    const bool editable = !read_only();
    const bool readable = text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD;
    const bool shift = event.IsShiftDown();
    const bool control = event.IsControlDown();
    const bool alt = event.IsAltDown() || event.IsAltGrDown();
    bool text_changed = false;
    bool cursor_changed = false;

    OnBeforeUserAction();
    switch (key_code) {
      case ui::VKEY_Z:
        if (control && !shift && !alt && editable)
          cursor_changed = text_changed = model_->Undo();
        else if (control && shift && !alt && editable)
          cursor_changed = text_changed = model_->Redo();
        break;
      case ui::VKEY_Y:
        if (control && !alt && editable)
          cursor_changed = text_changed = model_->Redo();
        break;
      case ui::VKEY_A:
        if (control && !alt) {
          model_->SelectAll(false);
          UpdateSelectionClipboard();
          cursor_changed = true;
        }
        break;
      case ui::VKEY_X:
        if (control && !alt && editable && readable)
          cursor_changed = text_changed = Cut();
        break;
      case ui::VKEY_C:
        if (control && !alt && readable)
          Copy();
        break;
      case ui::VKEY_V:
        if (control && !alt && editable)
          cursor_changed = text_changed = Paste();
        break;
      case ui::VKEY_RIGHT:
      case ui::VKEY_LEFT: {
        // We should ignore the alt-left/right keys because alt key doesn't make
        // any special effects for them and they can be shortcut keys such like
        // forward/back of the browser history.
        if (alt)
          break;
        const gfx::Range selection_range = render_text->selection();
        model_->MoveCursor(
            control ? gfx::WORD_BREAK : gfx::CHARACTER_BREAK,
            (key_code == ui::VKEY_RIGHT) ? gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT,
            shift);
        UpdateSelectionClipboard();
        cursor_changed = render_text->selection() != selection_range;
        break;
      }
      case ui::VKEY_END:
      case ui::VKEY_HOME:
        if ((key_code == ui::VKEY_HOME) ==
            (render_text->GetTextDirection() == base::i18n::RIGHT_TO_LEFT))
          model_->MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, shift);
        else
          model_->MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, shift);
        UpdateSelectionClipboard();
        cursor_changed = true;
        break;
      case ui::VKEY_BACK:
      case ui::VKEY_DELETE:
        if (!editable)
          break;
        if (!model_->HasSelection()) {
          gfx::VisualCursorDirection direction = (key_code == ui::VKEY_DELETE) ?
              gfx::CURSOR_RIGHT : gfx::CURSOR_LEFT;
          if (shift && control) {
            // If shift and control are pressed, erase up to the next line break
            // on Linux and ChromeOS. Otherwise, do nothing.
#if defined(OS_LINUX)
            model_->MoveCursor(gfx::LINE_BREAK, direction, true);
#else
            break;
#endif
          } else if (control) {
            // If only control is pressed, then erase the previous/next word.
            model_->MoveCursor(gfx::WORD_BREAK, direction, true);
          }
        }
        if (key_code == ui::VKEY_BACK)
          model_->Backspace();
        else if (shift && model_->HasSelection() && readable)
          Cut();
        else
          model_->Delete();

        // Consume backspace and delete keys even if the edit did nothing. This
        // prevents potential unintended side-effects of further event handling.
        text_changed = true;
        break;
      case ui::VKEY_INSERT:
        if (control && !shift && readable)
          Copy();
        else if (shift && !control && editable)
          cursor_changed = text_changed = Paste();
        break;
      default:
        break;
    }

    // We must have input method in order to support text input.
    DCHECK(GetInputMethod());
    UpdateAfterChange(text_changed, cursor_changed);
    OnAfterUserAction();
    return (text_changed || cursor_changed);
  }
  return false;
}

bool Textfield::OnMousePressed(const ui::MouseEvent& event) {
  TrackMouseClicks(event);

  if (!controller_ || !controller_->HandleMouseEvent(this, event)) {
    if (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) {
      RequestFocus();
      ShowImeIfNeeded();
    }

    if (event.IsOnlyLeftMouseButton()) {
      OnBeforeUserAction();
      initiating_drag_ = false;
      switch (aggregated_clicks_) {
        case 0:
          if (GetRenderText()->IsPointInSelection(event.location()))
            initiating_drag_ = true;
          else
            MoveCursorTo(event.location(), event.IsShiftDown());
          break;
        case 1:
          model_->MoveCursorTo(event.location(), false);
          model_->SelectWord();
          UpdateAfterChange(false, true);
          double_click_word_ = GetRenderText()->selection();
          break;
        case 2:
          SelectAll(false);
          break;
        default:
          NOTREACHED();
      }
      OnAfterUserAction();
    }

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    if (event.IsOnlyMiddleMouseButton()) {
      if (GetRenderText()->IsPointInSelection(event.location())) {
        OnBeforeUserAction();
        ClearSelection();
        ui::ScopedClipboardWriter(
            ui::Clipboard::GetForCurrentThread(),
            ui::CLIPBOARD_TYPE_SELECTION).WriteText(base::string16());
        OnAfterUserAction();
      } else if(!read_only()) {
        PasteSelectionClipboard(event);
      }
    }
#endif
  }

  touch_selection_controller_.reset();
  return true;
}

bool Textfield::OnMouseDragged(const ui::MouseEvent& event) {
  // Don't adjust the cursor on a potential drag and drop, or if the mouse
  // movement from the last mouse click does not exceed the drag threshold.
  if (initiating_drag_ || !event.IsOnlyLeftMouseButton() ||
      !ExceededDragThreshold(event.location() - last_click_location_)) {
    return true;
  }

  OnBeforeUserAction();
  model_->MoveCursorTo(event.location(), true);
  if (aggregated_clicks_ == 1) {
    model_->SelectWord();
    // Expand the selection so the initially selected word remains selected.
    gfx::Range selection = GetRenderText()->selection();
    const size_t min = std::min(selection.GetMin(),
                                double_click_word_.GetMin());
    const size_t max = std::max(selection.GetMax(),
                                double_click_word_.GetMax());
    const bool reversed = selection.is_reversed();
    selection.set_start(reversed ? max : min);
    selection.set_end(reversed ? min : max);
    model_->SelectRange(selection);
  }
  UpdateAfterChange(false, true);
  OnAfterUserAction();
  return true;
}

void Textfield::OnMouseReleased(const ui::MouseEvent& event) {
  OnBeforeUserAction();
  // Cancel suspected drag initiations, the user was clicking in the selection.
  if (initiating_drag_)
    MoveCursorTo(event.location(), false);
  initiating_drag_ = false;
  UpdateSelectionClipboard();
  OnAfterUserAction();
}

void Textfield::OnFocus() {
  GetRenderText()->set_focused(true);
  cursor_visible_ = true;
  SchedulePaint();
  GetInputMethod()->OnFocus();
  OnCaretBoundsChanged();

  const size_t caret_blink_ms = Textfield::GetCaretBlinkMs();
  if (caret_blink_ms != 0) {
    cursor_repaint_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(caret_blink_ms), this,
        &Textfield::UpdateCursor);
  }

  View::OnFocus();
  SchedulePaint();
}

void Textfield::OnBlur() {
  GetRenderText()->set_focused(false);
  GetInputMethod()->OnBlur();
  cursor_repaint_timer_.Stop();
  if (cursor_visible_) {
    cursor_visible_ = false;
    RepaintCursor();
  }

  touch_selection_controller_.reset();

  // Border typically draws focus indicator.
  SchedulePaint();
}

void Textfield::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TEXT;
  state->name = accessible_name_;
  if (read_only())
    state->state |= ui::AccessibilityTypes::STATE_READONLY;
  if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD)
    state->state |= ui::AccessibilityTypes::STATE_PROTECTED;
  state->value = text();

  const gfx::Range range = GetSelectedRange();
  state->selection_start = range.start();
  state->selection_end = range.end();

  if (!read_only()) {
    state->set_value_callback =
        base::Bind(&Textfield::AccessibilitySetValue,
                   weak_ptr_factory_.GetWeakPtr());
  }
}

ui::TextInputClient* Textfield::GetTextInputClient() {
  return read_only_ ? NULL : this;
}

gfx::Point Textfield::GetKeyboardContextMenuLocation() {
  return GetCaretBounds().bottom_right();
}

void Textfield::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void Textfield::OnEnabledChanged() {
  View::OnEnabledChanged();
  if (GetInputMethod())
    GetInputMethod()->OnTextInputTypeChanged(this);
  SchedulePaint();
}

const char* Textfield::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor Textfield::GetCursor(const ui::MouseEvent& event) {
  bool in_selection = GetRenderText()->IsPointInSelection(event.location());
  bool drag_event = event.type() == ui::ET_MOUSE_DRAGGED;
  bool text_cursor = !initiating_drag_ && (drag_event || !in_selection);
#if defined(USE_AURA)
  return text_cursor ? ui::kCursorIBeam : ui::kCursorNull;
#elif defined(OS_WIN)
  static HCURSOR ibeam = LoadCursor(NULL, IDC_IBEAM);
  static HCURSOR arrow = LoadCursor(NULL, IDC_ARROW);
  return text_cursor ? ibeam : arrow;
#endif
}

void Textfield::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      OnBeforeUserAction();
      RequestFocus();
      ShowImeIfNeeded();

      // We don't deselect if the point is in the selection
      // because TAP_DOWN may turn into a LONG_PRESS.
      if (!GetRenderText()->IsPointInSelection(event->location()))
        MoveCursorTo(event->location(), false);
      OnAfterUserAction();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      OnBeforeUserAction();
      MoveCursorTo(event->location(), true);
      OnAfterUserAction();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      CreateTouchSelectionControllerAndNotifyIt();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_TAP:
      if (event->details().tap_count() == 1) {
        CreateTouchSelectionControllerAndNotifyIt();
      } else {
        OnBeforeUserAction();
        SelectAll(false);
        OnAfterUserAction();
        event->SetHandled();
      }
#if defined(OS_WIN)
      if (!read_only())
        base::win::DisplayVirtualKeyboard();
#endif
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      // If long press happens outside selection, select word and show context
      // menu (If touch selection is enabled, context menu is shown by the
      // |touch_selection_controller_|, hence we mark the event handled.
      // Otherwise, the regular context menu will be shown by views).
      // If long press happens in selected text and touch drag drop is enabled,
      // we will turn off touch selection (if one exists) and let views do drag
      // drop.
      if (!GetRenderText()->IsPointInSelection(event->location())) {
        OnBeforeUserAction();
        model_->SelectWord();
        touch_selection_controller_.reset(
            ui::TouchSelectionController::create(this));
        UpdateAfterChange(false, true);
        OnAfterUserAction();
        if (touch_selection_controller_)
          event->SetHandled();
      } else if (switches::IsTouchDragDropEnabled()) {
        initiating_drag_ = true;
        touch_selection_controller_.reset();
      } else {
        if (!touch_selection_controller_)
          CreateTouchSelectionControllerAndNotifyIt();
        if (touch_selection_controller_)
          event->SetHandled();
      }
      return;
    case ui::ET_GESTURE_LONG_TAP:
      if (!touch_selection_controller_)
        CreateTouchSelectionControllerAndNotifyIt();

      // If touch selection is enabled, the context menu on long tap will be
      // shown by the |touch_selection_controller_|, hence we mark the event
      // handled so views does not try to show context menu on it.
      if (touch_selection_controller_)
        event->SetHandled();
      break;
    default:
      return;
  }
}

bool Textfield::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  if (!enabled() || read_only())
    return false;
  // TODO(msw): Can we support URL, FILENAME, etc.?
  *formats = ui::OSExchangeData::STRING;
  if (controller_)
    controller_->AppendDropFormats(formats, custom_formats);
  return true;
}

bool Textfield::CanDrop(const OSExchangeData& data) {
  int formats;
  std::set<OSExchangeData::CustomFormat> custom_formats;
  GetDropFormats(&formats, &custom_formats);
  return enabled() && !read_only() &&
      data.HasAnyFormat(formats, custom_formats);
}

int Textfield::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));
  gfx::RenderText* render_text = GetRenderText();
  const gfx::Range& selection = render_text->selection();
  drop_cursor_position_ = render_text->FindCursorPosition(event.location());
  bool in_selection = !selection.is_empty() &&
      selection.Contains(gfx::Range(drop_cursor_position_.caret_pos()));
  drop_cursor_visible_ = !in_selection;
  // TODO(msw): Pan over text when the user drags to the visible text edge.
  OnCaretBoundsChanged();
  SchedulePaint();

  if (initiating_drag_) {
    if (in_selection)
      return ui::DragDropTypes::DRAG_NONE;
    return event.IsControlDown() ? ui::DragDropTypes::DRAG_COPY :
                                   ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
}

void Textfield::OnDragExited() {
  drop_cursor_visible_ = false;
  SchedulePaint();
}

int Textfield::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));
  drop_cursor_visible_ = false;

  if (controller_) {
    int drag_operation = controller_->OnDrop(event.data());
    if (drag_operation != ui::DragDropTypes::DRAG_NONE)
      return drag_operation;
  }

  gfx::RenderText* render_text = GetRenderText();
  DCHECK(!initiating_drag_ ||
         !render_text->IsPointInSelection(event.location()));
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;

  gfx::SelectionModel drop_destination_model =
      render_text->FindCursorPosition(event.location());
  base::string16 new_text;
  event.data().GetString(&new_text);

  // Delete the current selection for a drag and drop within this view.
  const bool move = initiating_drag_ && !event.IsControlDown() &&
                    event.source_operations() & ui::DragDropTypes::DRAG_MOVE;
  if (move) {
    // Adjust the drop destination if it is on or after the current selection.
    size_t pos = drop_destination_model.caret_pos();
    pos -= render_text->selection().Intersect(gfx::Range(0, pos)).length();
    model_->DeleteSelectionAndInsertTextAt(new_text, pos);
  } else {
    model_->MoveCursorTo(drop_destination_model);
    // Drop always inserts text even if the textfield is not in insert mode.
    model_->InsertText(new_text);
  }
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
  return move ? ui::DragDropTypes::DRAG_MOVE : ui::DragDropTypes::DRAG_COPY;
}

void Textfield::OnDragDone() {
  initiating_drag_ = false;
  drop_cursor_visible_ = false;
}

void Textfield::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  GetRenderText()->SetDisplayRect(GetContentsBounds());
  OnCaretBoundsChanged();
}

void Textfield::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    UpdateColorsFromTheme(GetNativeTheme());
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, TextfieldModel::Delegate overrides:

void Textfield::OnCompositionTextConfirmedOrCleared() {
  if (!skip_input_method_cancel_composition_)
    GetInputMethod()->CancelComposition(this);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ContextMenuController overrides:

void Textfield::ShowContextMenuForView(
    View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  UpdateContextMenu();
  if (context_menu_runner_->RunMenuAt(GetWidget(), NULL,
          gfx::Rect(point, gfx::Size()), views::MenuItemView::TOPLEFT,
          source_type,
          MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU) ==
      MenuRunner::MENU_DELETED)
    return;
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, views::DragController overrides:

void Textfield::WriteDragDataForView(views::View* sender,
                                     const gfx::Point& press_pt,
                                     OSExchangeData* data) {
  DCHECK_NE(ui::DragDropTypes::DRAG_NONE,
            GetDragOperationsForView(sender, press_pt));
  data->SetString(model_->GetSelectedText());
  scoped_ptr<gfx::Canvas> canvas(
      views::GetCanvasForDragImage(GetWidget(), size()));
  GetRenderText()->DrawSelectedTextForDrag(canvas.get());
  drag_utils::SetDragImageOnDataObject(*canvas, size(),
                                       press_pt.OffsetFromOrigin(),
                                       data);
  if (controller_)
    controller_->OnWriteDragData(data);
}

int Textfield::GetDragOperationsForView(views::View* sender,
                                        const gfx::Point& p) {
  int drag_operations = ui::DragDropTypes::DRAG_COPY;
  if (!enabled() || text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD ||
      !GetRenderText()->IsPointInSelection(p)) {
    drag_operations = ui::DragDropTypes::DRAG_NONE;
  } else if (sender == this && !read_only()) {
    drag_operations =
        ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY;
  }
  if (controller_)
    controller_->OnGetDragOperationsForTextfield(&drag_operations);
  return drag_operations;
}

bool Textfield::CanStartDragForView(View* sender,
                                    const gfx::Point& press_pt,
                                    const gfx::Point& p) {
  return initiating_drag_ && GetRenderText()->IsPointInSelection(press_pt);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ui::TouchEditable overrides:

void Textfield::SelectRect(const gfx::Point& start, const gfx::Point& end) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  gfx::SelectionModel start_caret = GetRenderText()->FindCursorPosition(start);
  gfx::SelectionModel end_caret = GetRenderText()->FindCursorPosition(end);
  gfx::SelectionModel selection(
      gfx::Range(start_caret.caret_pos(), end_caret.caret_pos()),
      end_caret.caret_affinity());

  OnBeforeUserAction();
  SelectSelectionModel(selection);
  OnAfterUserAction();
}

void Textfield::MoveCaretTo(const gfx::Point& point) {
  SelectRect(point, point);
}

void Textfield::GetSelectionEndPoints(gfx::Rect* p1, gfx::Rect* p2) {
  gfx::RenderText* render_text = GetRenderText();
  const gfx::SelectionModel& sel = render_text->selection_model();
  gfx::SelectionModel start_sel =
      render_text->GetSelectionModelForSelectionStart();
  *p1 = render_text->GetCursorBounds(start_sel, true);
  *p2 = render_text->GetCursorBounds(sel, true);
}

gfx::Rect Textfield::GetBounds() {
  return GetLocalBounds();
}

gfx::NativeView Textfield::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

void Textfield::ConvertPointToScreen(gfx::Point* point) {
  View::ConvertPointToScreen(this, point);
}

void Textfield::ConvertPointFromScreen(gfx::Point* point) {
  View::ConvertPointFromScreen(this, point);
}

bool Textfield::DrawsHandles() {
  return false;
}

void Textfield::OpenContextMenu(const gfx::Point& anchor) {
  touch_selection_controller_.reset();
  ShowContextMenu(anchor, ui::MENU_SOURCE_TOUCH_EDIT_MENU);
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ui::SimpleMenuModel::Delegate overrides:

bool Textfield::IsCommandIdChecked(int command_id) const {
  return true;
}

bool Textfield::IsCommandIdEnabled(int command_id) const {
  base::string16 result;
  bool editable = !read_only();
  bool readable = text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD;
  switch (command_id) {
    case IDS_APP_UNDO:
      return editable && model_->CanUndo();
    case IDS_APP_CUT:
      return editable && readable && model_->HasSelection();
    case IDS_APP_COPY:
      return readable && model_->HasSelection();
    case IDS_APP_PASTE:
      ui::Clipboard::GetForCurrentThread()->ReadText(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
      return editable && !result.empty();
    case IDS_APP_DELETE:
      return editable && model_->HasSelection();
    case IDS_APP_SELECT_ALL:
      return !text().empty();
    default:
      return false;
  }
}

bool Textfield::GetAcceleratorForCommandId(int command_id,
                                           ui::Accelerator* accelerator) {
  return false;
}

void Textfield::ExecuteCommand(int command_id, int event_flags) {
  touch_selection_controller_.reset();
  if (!IsCommandIdEnabled(command_id))
    return;

  bool text_changed = false;
  OnBeforeUserAction();
  switch (command_id) {
    case IDS_APP_UNDO:
      text_changed = model_->Undo();
      break;
    case IDS_APP_CUT:
      text_changed = Cut();
      break;
    case IDS_APP_COPY:
      Copy();
      break;
    case IDS_APP_PASTE:
      text_changed = Paste();
      break;
    case IDS_APP_DELETE:
      text_changed = model_->Delete();
      break;
    case IDS_APP_SELECT_ALL:
      SelectAll(false);
      break;
    default:
      NOTREACHED();
      break;
  }
  UpdateAfterChange(text_changed, text_changed);
  OnAfterUserAction();
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, ui::TextInputClient overrides:

void Textfield::SetCompositionText(const ui::CompositionText& composition) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->SetCompositionText(composition);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void Textfield::ConfirmCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->ConfirmCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void Textfield::ClearCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->CancelCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void Textfield::InsertText(const base::string16& new_text) {
  // TODO(suzhe): Filter invalid characters.
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE || new_text.empty())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (GetRenderText()->insert_mode())
    model_->InsertText(new_text);
  else
    model_->ReplaceText(new_text);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void Textfield::InsertChar(base::char16 ch, int flags) {
  // Filter out all control characters, including tab and new line characters,
  // and all characters with Alt modifier. But allow characters with the AltGr
  // modifier. On Windows AltGr is represented by Alt+Ctrl, and on Linux it's a
  // different flag that we don't care about.
  const bool should_insert_char = ((ch >= 0x20 && ch < 0x7F) || ch > 0x9F) &&
      (flags & ~(ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN)) != ui::EF_ALT_DOWN;
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE || !should_insert_char)
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (GetRenderText()->insert_mode())
    model_->InsertChar(ch);
  else
    model_->ReplaceChar(ch);
  skip_input_method_cancel_composition_ = false;

  UpdateAfterChange(true, true);
  OnAfterUserAction();

  if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD &&
      password_reveal_duration_ != base::TimeDelta()) {
    const size_t change_offset = model_->GetCursorPosition();
    DCHECK_GT(change_offset, 0u);
    RevealPasswordChar(change_offset - 1);
  }
}

gfx::NativeWindow Textfield::GetAttachedWindow() const {
  // Imagine the following hierarchy.
  //   [NativeWidget A] - FocusManager
  //     [View]
  //     [NativeWidget B]
  //       [View]
  //         [View X]
  // An important thing is that [NativeWidget A] owns Win32 input focus even
  // when [View X] is logically focused by FocusManager. As a result, an Win32
  // IME may want to interact with the native view of [NativeWidget A] rather
  // than that of [NativeWidget B]. This is why we need to call
  // GetTopLevelWidget() here.
  return GetWidget()->GetTopLevelWidget()->GetNativeView();
}

ui::TextInputType Textfield::GetTextInputType() const {
  if (read_only() || !enabled())
    return ui::TEXT_INPUT_TYPE_NONE;
  return text_input_type_;
}

ui::TextInputMode Textfield::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

bool Textfield::CanComposeInline() const {
  return true;
}

gfx::Rect Textfield::GetCaretBounds() const {
  gfx::Rect rect = GetRenderText()->GetUpdatedCursorBounds();
  ConvertRectToScreen(this, &rect);
  return rect;
}

bool Textfield::GetCompositionCharacterBounds(uint32 index,
                                              gfx::Rect* rect) const {
  DCHECK(rect);
  if (!HasCompositionText())
    return false;
  gfx::RenderText* render_text = GetRenderText();
  const gfx::Range& composition_range = render_text->GetCompositionRange();
  DCHECK(!composition_range.is_empty());

  size_t text_index = composition_range.start() + index;
  if (composition_range.end() <= text_index)
    return false;
  if (!render_text->IsCursorablePosition(text_index)) {
    text_index = render_text->IndexOfAdjacentGrapheme(
        text_index, gfx::CURSOR_BACKWARD);
  }
  if (text_index < composition_range.start())
    return false;
  const gfx::SelectionModel caret(text_index, gfx::CURSOR_BACKWARD);
  *rect = render_text->GetCursorBounds(caret, false);
  ConvertRectToScreen(this, rect);
  return true;
}

bool Textfield::HasCompositionText() const {
  return model_->HasCompositionText();
}

bool Textfield::GetTextRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;

  model_->GetTextRange(range);
  return true;
}

bool Textfield::GetCompositionTextRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;

  model_->GetCompositionTextRange(range);
  return true;
}

bool Textfield::GetSelectionRange(gfx::Range* range) const {
  if (!ImeEditingAllowed())
    return false;
  *range = GetRenderText()->selection();
  return true;
}

bool Textfield::SetSelectionRange(const gfx::Range& range) {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;
  OnBeforeUserAction();
  SelectRange(range);
  OnAfterUserAction();
  return true;
}

bool Textfield::DeleteRange(const gfx::Range& range) {
  if (!ImeEditingAllowed() || range.is_empty())
    return false;

  OnBeforeUserAction();
  model_->SelectRange(range);
  if (model_->HasSelection()) {
    model_->DeleteSelection();
    UpdateAfterChange(true, true);
  }
  OnAfterUserAction();
  return true;
}

bool Textfield::GetTextFromRange(const gfx::Range& range,
                                 base::string16* range_text) const {
  if (!ImeEditingAllowed() || !range.IsValid())
    return false;

  gfx::Range text_range;
  if (!GetTextRange(&text_range) || !text_range.Contains(range))
    return false;

  *range_text = model_->GetTextFromRange(range);
  return true;
}

void Textfield::OnInputMethodChanged() {}

bool Textfield::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  // Restore text directionality mode when the indicated direction matches the
  // current forced mode; otherwise, force the mode indicated. This helps users
  // manage BiDi text layout without getting stuck in forced LTR or RTL modes.
  const gfx::DirectionalityMode mode = direction == base::i18n::RIGHT_TO_LEFT ?
      gfx::DIRECTIONALITY_FORCE_RTL : gfx::DIRECTIONALITY_FORCE_LTR;
  if (mode == GetRenderText()->directionality_mode())
    GetRenderText()->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_TEXT);
  else
    GetRenderText()->SetDirectionalityMode(mode);
  SchedulePaint();
  return true;
}

void Textfield::ExtendSelectionAndDelete(size_t before, size_t after) {
  gfx::Range range = GetRenderText()->selection();
  DCHECK_GE(range.start(), before);

  range.set_start(range.start() - before);
  range.set_end(range.end() + after);
  gfx::Range text_range;
  if (GetTextRange(&text_range) && text_range.Contains(range))
    DeleteRange(range);
}

void Textfield::EnsureCaretInRect(const gfx::Rect& rect) {}

void Textfield::OnCandidateWindowShown() {}

void Textfield::OnCandidateWindowUpdated() {}

void Textfield::OnCandidateWindowHidden() {}

////////////////////////////////////////////////////////////////////////////////
// Textfield, protected:

gfx::RenderText* Textfield::GetRenderText() const {
  return model_->render_text();
}

base::string16 Textfield::GetSelectionClipboardText() const {
  base::string16 selection_clipboard_text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::CLIPBOARD_TYPE_SELECTION, &selection_clipboard_text);
  return selection_clipboard_text;
}

////////////////////////////////////////////////////////////////////////////////
// Textfield, private:

void Textfield::AccessibilitySetValue(const base::string16& new_value) {
  if (!read_only()) {
    SetText(new_value);
    ClearSelection();
  }
}

void Textfield::UpdateBackgroundColor() {
  const SkColor color = GetBackgroundColor();
  set_background(Background::CreateSolidBackground(color));
  GetRenderText()->set_background_is_transparent(SkColorGetA(color) != 0xFF);
  SchedulePaint();
}

void Textfield::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  gfx::RenderText* render_text = GetRenderText();
  render_text->SetColor(GetTextColor());
  UpdateBackgroundColor();
  render_text->set_cursor_color(GetTextColor());
  render_text->set_selection_color(theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionColor));
  render_text->set_selection_background_focused_color(theme->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused));
}

void Textfield::UpdateAfterChange(bool text_changed, bool cursor_changed) {
  if (text_changed) {
    if (controller_)
      controller_->ContentsChanged(this, text());
    NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
  }
  if (cursor_changed) {
    cursor_visible_ = true;
    RepaintCursor();
    if (cursor_repaint_timer_.IsRunning())
    cursor_repaint_timer_.Reset();
    if (!text_changed) {
      // TEXT_CHANGED implies SELECTION_CHANGED, so we only need to fire
      // this if only the selection changed.
      NotifyAccessibilityEvent(
          ui::AccessibilityTypes::EVENT_SELECTION_CHANGED, true);
    }
  }
  if (text_changed || cursor_changed) {
    OnCaretBoundsChanged();
    SchedulePaint();
  }
}

void Textfield::UpdateCursor() {
  const size_t caret_blink_ms = Textfield::GetCaretBlinkMs();
  cursor_visible_ = !cursor_visible_ || (caret_blink_ms == 0);
  RepaintCursor();
}

void Textfield::RepaintCursor() {
  gfx::Rect r(GetRenderText()->GetUpdatedCursorBounds());
  r.Inset(-1, -1, -1, -1);
  SchedulePaintInRect(r);
}

void Textfield::PaintTextAndCursor(gfx::Canvas* canvas) {
  TRACE_EVENT0("views", "Textfield::PaintTextAndCursor");
  canvas->Save();

  // Draw placeholder text if needed.
  gfx::RenderText* render_text = GetRenderText();
  if (text().empty() && !GetPlaceholderText().empty()) {
    canvas->DrawStringRect(GetPlaceholderText(), GetFontList(),
        placeholder_text_color(), render_text->display_rect());
  }

  // Draw the text, cursor, and selection.
  render_text->set_cursor_visible(cursor_visible_ && !drop_cursor_visible_ &&
                                  !HasSelection());
  render_text->Draw(canvas);

  // Draw the detached drop cursor that marks where the text will be dropped.
  if (drop_cursor_visible_)
    render_text->DrawCursor(canvas, drop_cursor_position_);

  canvas->Restore();
}

void Textfield::MoveCursorTo(const gfx::Point& point, bool select) {
  if (model_->MoveCursorTo(point, select))
    UpdateAfterChange(false, true);
}

void Textfield::OnCaretBoundsChanged() {
  if (GetInputMethod())
    GetInputMethod()->OnCaretBoundsChanged(this);
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void Textfield::OnBeforeUserAction() {
  if (controller_)
    controller_->OnBeforeUserAction(this);
}

void Textfield::OnAfterUserAction() {
  if (controller_)
    controller_->OnAfterUserAction(this);
}

bool Textfield::Cut() {
  if (!read_only() && text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD &&
      model_->Cut()) {
    if (controller_)
      controller_->OnAfterCutOrCopy();
    return true;
  }
  return false;
}

bool Textfield::Copy() {
  if (text_input_type_ != ui::TEXT_INPUT_TYPE_PASSWORD && model_->Copy()) {
    if (controller_)
      controller_->OnAfterCutOrCopy();
    return true;
  }
  return false;
}

bool Textfield::Paste() {
  if (!read_only() && model_->Paste()) {
    if (controller_)
      controller_->OnAfterPaste();
    return true;
  }
  return false;
}

void Textfield::UpdateContextMenu() {
  if (!context_menu_contents_.get()) {
    context_menu_contents_.reset(new ui::SimpleMenuModel(this));
    context_menu_contents_->AddItemWithStringId(IDS_APP_UNDO, IDS_APP_UNDO);
    context_menu_contents_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_contents_->AddItemWithStringId(IDS_APP_CUT, IDS_APP_CUT);
    context_menu_contents_->AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
    context_menu_contents_->AddItemWithStringId(IDS_APP_PASTE, IDS_APP_PASTE);
    context_menu_contents_->AddItemWithStringId(IDS_APP_DELETE, IDS_APP_DELETE);
    context_menu_contents_->AddSeparator(ui::NORMAL_SEPARATOR);
    context_menu_contents_->AddItemWithStringId(IDS_APP_SELECT_ALL,
                                                IDS_APP_SELECT_ALL);
    if (controller_)
      controller_->UpdateContextMenu(context_menu_contents_.get());
  }
  context_menu_runner_.reset(new MenuRunner(context_menu_contents_.get()));
}

void Textfield::TrackMouseClicks(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    base::TimeDelta time_delta = event.time_stamp() - last_click_time_;
    if (time_delta.InMilliseconds() <= GetDoubleClickInterval() &&
        !ExceededDragThreshold(event.location() - last_click_location_)) {
      // Upon clicking after a triple click, the count should go back to double
      // click and alternate between double and triple. This assignment maps
      // 0 to 1, 1 to 2, 2 to 1.
      aggregated_clicks_ = (aggregated_clicks_ % 2) + 1;
    } else {
      aggregated_clicks_ = 0;
    }
    last_click_time_ = event.time_stamp();
    last_click_location_ = event.location();
  }
}

bool Textfield::ImeEditingAllowed() const {
  // Disallow input method editing of password fields.
  ui::TextInputType t = GetTextInputType();
  return (t != ui::TEXT_INPUT_TYPE_NONE && t != ui::TEXT_INPUT_TYPE_PASSWORD);
}

void Textfield::RevealPasswordChar(int index) {
  GetRenderText()->SetObscuredRevealIndex(index);
  SchedulePaint();

  if (index != -1) {
    password_reveal_timer_.Start(FROM_HERE, password_reveal_duration_,
        base::Bind(&Textfield::RevealPasswordChar,
                   weak_ptr_factory_.GetWeakPtr(), -1));
  }
}

void Textfield::CreateTouchSelectionControllerAndNotifyIt() {
  if (!touch_selection_controller_) {
    touch_selection_controller_.reset(
        ui::TouchSelectionController::create(this));
  }
  if (touch_selection_controller_)
    touch_selection_controller_->SelectionChanged();
}

void Textfield::UpdateSelectionClipboard() const {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (HasSelection()) {
    ui::ScopedClipboardWriter(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_SELECTION).WriteText(GetSelectedText());
  }
#endif
}

void Textfield::PasteSelectionClipboard(const ui::MouseEvent& event) {
  DCHECK(event.IsOnlyMiddleMouseButton());
  DCHECK(!read_only());
  base::string16 selection_clipboard_text = GetSelectionClipboardText();
  if (!selection_clipboard_text.empty()) {
    OnBeforeUserAction();
    gfx::Range range = GetSelectionModel().selection();
    gfx::LogicalCursorDirection affinity = GetSelectionModel().caret_affinity();
    const gfx::SelectionModel mouse =
        GetRenderText()->FindCursorPosition(event.location());
    model_->MoveCursorTo(mouse);
    model_->InsertText(selection_clipboard_text);
    // Update the new selection range as needed.
    if (range.GetMin() >= mouse.caret_pos()) {
      const size_t length = selection_clipboard_text.length();
      range = gfx::Range(range.start() + length, range.end() + length);
    }
    model_->MoveCursorTo(gfx::SelectionModel(range, affinity));
    UpdateAfterChange(true, true);
    OnAfterUserAction();
  }
}

}  // namespace views
