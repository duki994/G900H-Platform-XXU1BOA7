// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_

#include "components/translate/core/browser/translate_driver.h"

#include "components/translate/core/browser/language_state.h"

namespace content {
struct LoadCommittedDetails;
class NavigationController;
class WebContents;
}

// Content implementation of TranslateDriver.
class ContentTranslateDriver : public TranslateDriver {
 public:

  // The observer for the ContentTranslateDriver.
  class Observer {
   public:
    // Handles when the value of IsPageTranslated is changed.
    virtual void OnIsPageTranslatedChanged(content::WebContents* source) = 0;

    // Handles when the value of translate_enabled is changed.
    virtual void OnTranslateEnabledChanged(content::WebContents* source) = 0;

   protected:
    virtual ~Observer() {}
  };

  ContentTranslateDriver(content::NavigationController* nav_controller);
  virtual ~ContentTranslateDriver();

  // Gets the language state.
  LanguageState& language_state() { return language_state_; }

  // Sets the Observer. Calling this method is optional.
  void set_observer(Observer* observer) { observer_ = observer; }

  // Must be called on navigations.
  void DidNavigate(const content::LoadCommittedDetails& details);

 private:
  // TranslateDriver methods
  virtual void OnIsPageTranslatedChanged() OVERRIDE;
  virtual void OnTranslateEnabledChanged() OVERRIDE;
  virtual bool IsLinkNavigation() OVERRIDE;

  // The navigation controller of the tab we are associated with.
  content::NavigationController* navigation_controller_;

  LanguageState language_state_;
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ContentTranslateDriver);
};

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTENT_TRANSLATE_DRIVER_H_
