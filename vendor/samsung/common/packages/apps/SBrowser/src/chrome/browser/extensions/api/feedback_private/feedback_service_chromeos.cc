// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace extensions {

class FeedbackServiceImpl
    : public FeedbackService,
      public base::SupportsWeakPtr<FeedbackServiceImpl> {
 public:
  FeedbackServiceImpl();
  virtual ~FeedbackServiceImpl();

  virtual std::string GetUserEmail() OVERRIDE;
  virtual void GetHistograms(std::string* histograms) OVERRIDE;

 private:
  // Overridden from FeedbackService:
  virtual base::WeakPtr<FeedbackService> GetWeakPtr() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FeedbackServiceImpl);
};

FeedbackService* FeedbackService::CreateInstance() {
  return new FeedbackServiceImpl;
}

FeedbackServiceImpl::FeedbackServiceImpl() {
}

FeedbackServiceImpl::~FeedbackServiceImpl() {
}

std::string FeedbackServiceImpl::GetUserEmail() {
  chromeos::UserManager* manager = chromeos::UserManager::Get();
  if (!manager)
    return std::string();
  else
    return manager->GetLoggedInUser()->display_email();
}

void FeedbackServiceImpl::GetHistograms(std::string* histograms) {
  *histograms = base::StatisticsRecorder::ToJSON(std::string());
}

base::WeakPtr<FeedbackService> FeedbackServiceImpl::GetWeakPtr() {
  return AsWeakPtr();
}

}  // namespace extensions
