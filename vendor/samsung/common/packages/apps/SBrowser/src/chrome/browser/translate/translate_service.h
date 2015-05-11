// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_

#include "chrome/browser/web_resource/resource_request_allowed_notifier.h"

class PrefService;

// Singleton managing the resources required for Translate.
class TranslateService : public ResourceRequestAllowedNotifier::Observer {
 public:
   // Must be called before the Translate feature can be used.
  static void Initialize();

  // Must be called to shut down the Translate feature.
  static void Shutdown(bool cleanup_pending_fetcher);

  // Let the caller decide if and when we should fetch the language list from
  // the translate server. This is a NOOP if switches::kDisableTranslate is set
  // or if prefs::kEnableTranslate is set to false.
  static void FetchLanguageListFromTranslateServer(PrefService* prefs);

  // Returns true if the new translate bubble is enabled.
  static bool IsTranslateBubbleEnabled();

  // Sets whether of not the infobar UI is used. This method is intended to be
  // used only for tests.
  static void SetUseInfobar(bool value);

 private:
  TranslateService();
  ~TranslateService();

  // ResourceRequestAllowedNotifier::Observer methods.
  virtual void OnResourceRequestsAllowed() OVERRIDE;

  // Helper class to know if it's allowed to make network resource requests.
  ResourceRequestAllowedNotifier resource_request_allowed_notifier_;

  // Whether or not the infobar is used. This is intended to be used
  // only for testing.
  bool use_infobar_;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_SERVICE_H_
