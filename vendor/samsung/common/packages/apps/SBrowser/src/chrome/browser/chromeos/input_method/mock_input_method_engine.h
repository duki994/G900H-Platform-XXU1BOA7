// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_

#include <string>
#include <vector>
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "url/gurl.h"

namespace ui {
class KeyEvent;
}  // namespace ui

namespace chromeos {

class CompositionText;

namespace input_method {
class CandidateWindow;
struct InputMethodProperty;
struct KeyEventHandle;
}  // namespace input_method

class MockInputMethodEngine : public InputMethodEngineInterface {
 public:
  explicit MockInputMethodEngine(
      const input_method::InputMethodDescriptor& descriptor);
  virtual ~MockInputMethodEngine();

  // InputMethodEngineInterface overrides.
  virtual const input_method::InputMethodDescriptor& GetDescriptor()
      const OVERRIDE;
  virtual void StartIme() OVERRIDE;
  virtual bool SetComposition(int context_id,
                              const char* text,
                              int selection_start,
                              int selection_end,
                              int cursor,
                              const std::vector<SegmentInfo>& segments,
                              std::string* error) OVERRIDE;
  virtual bool ClearComposition(int context_id, std::string* error) OVERRIDE;
  virtual bool CommitText(int context_id, const char* text,
                          std::string* error) OVERRIDE;
  virtual bool SendKeyEvents(int context_id,
                             const std::vector<KeyboardEvent>& events) OVERRIDE;
  virtual const CandidateWindowProperty&
    GetCandidateWindowProperty() const OVERRIDE;
  virtual void SetCandidateWindowProperty(
      const CandidateWindowProperty& property) OVERRIDE;
  virtual bool SetCandidateWindowVisible(bool visible,
                                         std::string* error) OVERRIDE;
  virtual bool SetCandidates(int context_id,
                             const std::vector<Candidate>& candidates,
                             std::string* error) OVERRIDE;
  virtual bool SetCursorPosition(int context_id, int candidate_id,
                                 std::string* error) OVERRIDE;
  virtual bool SetMenuItems(const std::vector<MenuItem>& items) OVERRIDE;
  virtual bool UpdateMenuItems(const std::vector<MenuItem>& items) OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void KeyEventDone(input_method::KeyEventHandle* key_data,
                            bool handled) OVERRIDE;
  virtual bool DeleteSurroundingText(int context_id,
                                     int offset,
                                     size_t number_of_chars,
                                     std::string* error) OVERRIDE;

  // IMEEngineHandlerInterface overrides.
  virtual void FocusIn(
      const IMEEngineHandlerInterface::InputContext& input_context) OVERRIDE;
  virtual void FocusOut() OVERRIDE;
  virtual void Enable() OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void PropertyActivate(const std::string& property_name) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void ProcessKeyEvent(const ui::KeyEvent& key_event,
                               const KeyEventDoneCallback& callback) OVERRIDE;
  virtual void CandidateClicked(uint32 index) OVERRIDE;
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) OVERRIDE;
  virtual void HideInputView() OVERRIDE;

 private:
  // Descriptor of this input method.
  input_method::InputMethodDescriptor descriptor_;

  // The current candidate window property.
  CandidateWindowProperty candidate_window_property_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_INPUT_METHOD_ENGINE_H_
