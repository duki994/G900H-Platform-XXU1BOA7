// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feedback/feedback_uploader.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace feedback {

// static
FeedbackUploaderFactory* FeedbackUploaderFactory::GetInstance() {
  return Singleton<FeedbackUploaderFactory>::get();
}

// static
FeedbackUploader* FeedbackUploaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FeedbackUploader*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

FeedbackUploaderFactory::FeedbackUploaderFactory()
    : BrowserContextKeyedServiceFactory(
          "feedback::FeedbackUploader",
          BrowserContextDependencyManager::GetInstance()) {}

FeedbackUploaderFactory::~FeedbackUploaderFactory() {}

BrowserContextKeyedService* FeedbackUploaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new FeedbackUploader(context);
}

content::BrowserContext* FeedbackUploaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

}  // namespace feedback
