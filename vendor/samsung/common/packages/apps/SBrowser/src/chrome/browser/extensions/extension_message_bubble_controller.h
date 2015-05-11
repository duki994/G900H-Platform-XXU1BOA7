// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_CONTROLLER_H_

#include <string>
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "extensions/common/extension.h"

class Browser;
class ExtensionService;

namespace extensions {

class ExtensionPrefs;
class SuspiciousExtensionBubble;

class ExtensionMessageBubbleController {
 public:
  // UMA histogram constants.
  enum BubbleAction {
    ACTION_LEARN_MORE = 0,
    ACTION_EXECUTE,
    ACTION_DISMISS,
    ACTION_BOUNDARY, // Must be the last value.
  };

  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    virtual bool ShouldIncludeExtension(const std::string& extension_id) = 0;
    virtual void AcknowledgeExtension(
        const std::string& extension_id,
        BubbleAction action) = 0;
    virtual void PerformAction(const ExtensionIdList& list) = 0;

    // Text for various UI labels shown in the bubble.
    virtual base::string16 GetTitle() const = 0;
    virtual base::string16 GetMessageBody() const = 0;
    virtual base::string16 GetOverflowText(
        const base::string16& overflow_count) const = 0;
    virtual base::string16 GetLearnMoreLabel() const = 0;
    virtual GURL GetLearnMoreUrl() const = 0;
    virtual base::string16 GetActionButtonLabel() const = 0;
    virtual base::string16 GetDismissButtonLabel() const = 0;

    // Whether to show a list of extensions in the bubble.
    virtual bool ShouldShowExtensionList() const = 0;

    // Record, through UMA, how many extensions were found.
    virtual void LogExtensionCount(size_t count) = 0;
    virtual void LogAction(BubbleAction action) = 0;
  };

  ExtensionMessageBubbleController(Delegate* delegate, Profile* profile);
  virtual ~ExtensionMessageBubbleController();

  Delegate* delegate() const { return delegate_.get(); }

  // Obtains a list of all extensions (by name) the controller knows about.
  std::vector<base::string16> GetExtensionList();

  // Obtains a list of all extensions (by id) the controller knows about.
  const ExtensionIdList& GetExtensionIdList();

  // Sets up the callbacks and shows the bubble.
  virtual void Show(ExtensionMessageBubble* bubble);

  // Callbacks from bubble. Declared virtual for testing purposes.
  virtual void OnBubbleAction();
  virtual void OnBubbleDismiss();
  virtual void OnLinkClicked();

 private:
  // Iterate over the known extensions and acknowledge each one.
  void AcknowledgeExtensions();

  // Get the data this class needs.
  ExtensionIdList* GetOrCreateExtensionList();

  // Our extension service. Weak, not owned by us.
  ExtensionService* service_;

  // A weak pointer to the profile we are associated with. Not owned by us.
  Profile* profile_;

  // The list of extensions found.
  ExtensionIdList extension_list_;

  // The action the user took in the bubble.
  BubbleAction user_action_;

  // Our delegate supplying information about what to show in the dialog.
  scoped_ptr<Delegate> delegate_;

  // Whether this class has initialized.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_CONTROLLER_H_
