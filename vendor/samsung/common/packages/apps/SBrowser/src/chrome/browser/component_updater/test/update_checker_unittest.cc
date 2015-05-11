// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/version.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "chrome/browser/component_updater/test/component_updater_service_unittest.h"
#include "chrome/browser/component_updater/test/url_request_post_interceptor.h"
#include "chrome/browser/component_updater/update_checker.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace component_updater {

namespace {

base::FilePath test_file(const char* file) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.AppendASCII("components").AppendASCII(file);
}

}  // namespace

class UpdateCheckerTest : public testing::Test {
 public:
  UpdateCheckerTest();
  virtual ~UpdateCheckerTest();

  // Overrides from testing::Test.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void UpdateCheckComplete(int error,
                           const std::string& error_message,
                           const UpdateResponse::Results& results);

  net::URLRequestContextGetter* context() {
    return context_.get();
  }

 protected:
  void Quit();
  void RunThreads();
  void RunThreadsUntilIdle();

  CrxUpdateItem BuildCrxUpdateItem();

  scoped_ptr<UpdateChecker> update_checker_;

  scoped_ptr<InterceptorFactory> interceptor_factory_;
  URLRequestPostInterceptor* post_interceptor_;   // Owned by the factory.

  int error_;
  std::string error_message_;
  UpdateResponse::Results results_;

 private:
  scoped_refptr<net::TestURLRequestContextGetter> context_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::FilePath test_data_dir_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckerTest);
};

UpdateCheckerTest::UpdateCheckerTest()
    : error_(0),
      context_(new net::TestURLRequestContextGetter(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))),
      thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {
  // The test directory is chrome/test/data/components.
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("components");

  net::URLFetcher::SetEnableInterceptionForTests(true);
}

UpdateCheckerTest::~UpdateCheckerTest() {
  net::URLFetcher::SetEnableInterceptionForTests(false);
  context_ = NULL;
}

void UpdateCheckerTest::SetUp() {
  interceptor_factory_.reset(new InterceptorFactory);
  post_interceptor_ = interceptor_factory_->CreateInterceptor();
  EXPECT_TRUE(post_interceptor_);

  update_checker_.reset();

  error_ = 0;
  error_message_.clear();
  results_ = UpdateResponse::Results();
}

void UpdateCheckerTest::TearDown() {
  update_checker_.reset();

  post_interceptor_ = NULL;
  interceptor_factory_.reset();
}

void UpdateCheckerTest::RunThreads() {
  base::RunLoop runloop;
  quit_closure_ = runloop.QuitClosure();
  runloop.Run();

  // Since some tests need to drain currently enqueued tasks such as network
  // intercepts on the IO thread, run the threads until they are
  // idle. The component updater service won't loop again until the loop count
  // is set and the service is started.
  RunThreadsUntilIdle();
}

void UpdateCheckerTest::RunThreadsUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void UpdateCheckerTest::Quit() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void UpdateCheckerTest::UpdateCheckComplete(
    int error,
    const std::string& error_message,
    const UpdateResponse::Results& results) {
  error_ = error;
  error_message_ = error_message;
  results_ = results;
  Quit();
}

CrxUpdateItem UpdateCheckerTest::BuildCrxUpdateItem() {
  CrxComponent crx_component;
  crx_component.name = "test_jebg";
  crx_component.pk_hash.assign(jebg_hash, jebg_hash + arraysize(jebg_hash));
  crx_component.installer = NULL;
  crx_component.observer = NULL;
  crx_component.version = base::Version("0.9");
  crx_component.fingerprint = "fp1";

  CrxUpdateItem crx_update_item;
  crx_update_item.status = CrxUpdateItem::kNew;
  crx_update_item.id = "jebgalgnebhfojomionfpkfelancnnkf";
  crx_update_item.component = crx_component;

  return crx_update_item;
}

TEST_F(UpdateCheckerTest, UpdateCheckSuccess) {
  EXPECT_TRUE(post_interceptor_->ExpectRequest(new PartialMatch(
      "updatecheck"), test_file("updatecheck_reply_1.xml")));

  update_checker_ = UpdateChecker::Create(
      GURL("http://localhost2/update2"),
      context(),
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this))).Pass();

  CrxUpdateItem item(BuildCrxUpdateItem());
  std::vector<CrxUpdateItem*> items_to_check;
  items_to_check.push_back(&item);

  update_checker_->CheckForUpdates(items_to_check, "extra=\"params\"");

  RunThreads();

  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  // Sanity check the request.
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "request protocol=\"3.0\" extra=\"params\""));
  EXPECT_NE(string::npos, post_interceptor_->GetRequests()[0].find(
      "app appid=\"jebgalgnebhfojomionfpkfelancnnkf\" version=\"0.9\">"
      "<updatecheck /><packages><package fp=\"fp1\"/></packages></app>"));

  // Sanity check the arguments of the callback after parsing.
  EXPECT_EQ(0, error_);
  EXPECT_TRUE(error_message_.empty());
  EXPECT_EQ(1ul, results_.list.size());
  EXPECT_STREQ("jebgalgnebhfojomionfpkfelancnnkf",
               results_.list[0].extension_id.c_str());
  EXPECT_STREQ("1.0", results_.list[0].manifest.version.c_str());
}

TEST_F(UpdateCheckerTest, UpdateNetworkError) {
  // Setting this expectation simulates a network error since the
  // file is not found. Since setting the expectation fails, this function
  // owns |request_matcher|.
  scoped_ptr<PartialMatch> request_matcher( new PartialMatch("updatecheck"));
  EXPECT_FALSE(post_interceptor_->ExpectRequest(request_matcher.get(),
                                                test_file("no such file")));

  update_checker_ = UpdateChecker::Create(
      GURL("http://localhost2/update2"),
      context(),
      base::Bind(&UpdateCheckerTest::UpdateCheckComplete,
                 base::Unretained(this))).Pass();

  CrxUpdateItem item(BuildCrxUpdateItem());
  std::vector<CrxUpdateItem*> items_to_check;
  items_to_check.push_back(&item);

  update_checker_->CheckForUpdates(items_to_check, "");

  RunThreads();

  EXPECT_EQ(0, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  EXPECT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  EXPECT_NE(0, error_);
  EXPECT_STREQ("network error", error_message_.c_str());
  EXPECT_EQ(0ul, results_.list.size());
}

}  // namespace component_updater

