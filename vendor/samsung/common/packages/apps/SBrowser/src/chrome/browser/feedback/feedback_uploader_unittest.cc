// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader.h"

#include <set>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/feedback/feedback_uploader_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kReportOne[] = "one";
const char kReportTwo[] = "two";
const char kReportThree[] = "three";
const char kReportFour[] = "four";
const char kReportFive[] = "five";

const base::TimeDelta kRetryDelayForTest =
    base::TimeDelta::FromMilliseconds(100);

BrowserContextKeyedService* CreateFeedbackUploaderService(
    content::BrowserContext* context) {
  return new feedback::FeedbackUploader(Profile::FromBrowserContext(context));
}

}  // namespace

namespace feedback {

class FeedbackUploaderTest : public testing::Test {
 protected:
  FeedbackUploaderTest()
     : ui_thread_(content::BrowserThread::UI, &message_loop_),
       profile_(new TestingProfile()),
       dispatched_reports_count_(0),
       expected_reports_(0) {
    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        profile_.get(), &CreateFeedbackUploaderService);

    uploader_ = FeedbackUploaderFactory::GetForBrowserContext(profile_.get());
    uploader_->setup_for_test(
        base::Bind(&FeedbackUploaderTest::MockDispatchReport,
                   base::Unretained(this)),
        kRetryDelayForTest);
  }

  virtual ~FeedbackUploaderTest() {
    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        profile_.get(), NULL);
  }

  void QueueReport(const std::string& data) {
    uploader_->QueueReport(data);
  }

  void ReportFailure(const std::string& data) {
    uploader_->RetryReport(data);
  }

  void MockDispatchReport(const std::string& report_data) {
    if (ContainsKey(dispatched_reports_, report_data)) {
      dispatched_reports_[report_data]++;
    } else {
      dispatched_reports_[report_data] = 1;
    }
    dispatched_reports_count_++;

    // Dispatch will always update the timer, whether successful or not,
    // simulate the same behavior.
    uploader_->UpdateUploadTimer();

    if (ProcessingComplete()) {
      if (run_loop_.get())
        run_loop_->Quit();
    }
  }

  bool ProcessingComplete() {
    return (dispatched_reports_count_ >= expected_reports_);
  }

  void RunMessageLoop() {
    if (ProcessingComplete())
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;

  FeedbackUploader* uploader_;

  std::map<std::string, unsigned int> dispatched_reports_;
  size_t dispatched_reports_count_;
  size_t expected_reports_;
};

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
#define MAYBE_QueueMultiple QueueMultiple
#else
// crbug.com/330547
#define MAYBE_QueueMultiple DISABLED_QueueMultiple
#endif
TEST_F(FeedbackUploaderTest, MAYBE_QueueMultiple) {
  dispatched_reports_.clear();
  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);
  QueueReport(kReportFour);

  EXPECT_EQ(dispatched_reports_.size(), 4u);
  EXPECT_EQ(dispatched_reports_[kReportOne], 1u);
  EXPECT_EQ(dispatched_reports_[kReportTwo], 1u);
  EXPECT_EQ(dispatched_reports_[kReportThree], 1u);
  EXPECT_EQ(dispatched_reports_[kReportFour], 1u);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
#define MAYBE_QueueMultipleWithFailures QueueMultipleWithFailures
#else
// crbug.com/330547
#define MAYBE_QueueMultipleWithFailures DISABLED_QueueMultipleWithFailures
#endif
TEST_F(FeedbackUploaderTest, MAYBE_QueueMultipleWithFailures) {
  dispatched_reports_.clear();

  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);
  QueueReport(kReportFour);

  ReportFailure(kReportThree);
  ReportFailure(kReportTwo);
  QueueReport(kReportFive);

  expected_reports_ = 7;
  RunMessageLoop();

  EXPECT_EQ(dispatched_reports_.size(), 5u);
  EXPECT_EQ(dispatched_reports_[kReportOne], 1u);
  EXPECT_EQ(dispatched_reports_[kReportTwo], 2u);
  EXPECT_EQ(dispatched_reports_[kReportThree], 2u);
  EXPECT_EQ(dispatched_reports_[kReportFour], 1u);
  EXPECT_EQ(dispatched_reports_[kReportFive], 1u);
}

}  // namespace feedback
