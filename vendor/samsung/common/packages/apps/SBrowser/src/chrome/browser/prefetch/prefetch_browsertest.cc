// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

namespace {

const char kPrefetchPage[] = "files/prerender/simple_prefetch.html";

class PrefetchBrowserTestBase : public InProcessBrowserTest {
 public:
  explicit PrefetchBrowserTestBase(bool do_predictive_networking,
                                   bool do_prefetch_field_trial)
      : do_predictive_networking_(do_predictive_networking),
        do_prefetch_field_trial_(do_prefetch_field_trial) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    if (do_prefetch_field_trial_) {
      command_line->AppendSwitchASCII(
          switches::kForceFieldTrials,
          "Prefetch/ExperimentYes/");
    } else {
      command_line->AppendSwitchASCII(
          switches::kForceFieldTrials,
          "Prefetch/ExperimentNo/");
    }
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kNetworkPredictionEnabled, do_predictive_networking_);
  }

  bool RunPrefetchExperiment(bool expect_success, Browser* browser) {
    CHECK(test_server()->Start());
    GURL url = test_server()->GetURL(kPrefetchPage);

    const base::string16 expected_title =
        expect_success ? base::ASCIIToUTF16("link onload")
                       : base::ASCIIToUTF16("link onerror");
    content::TitleWatcher title_watcher(
        browser->tab_strip_model()->GetActiveWebContents(), expected_title);
    ui_test_utils::NavigateToURL(browser, url);
    return expected_title == title_watcher.WaitAndGetTitle();
  }

 private:
  bool do_predictive_networking_;
  bool do_prefetch_field_trial_;
};

class PrefetchBrowserTestPredictionOnExpOn : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOnExpOn()
      : PrefetchBrowserTestBase(true, true) {}
};

class PrefetchBrowserTestPredictionOnExpOff : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOnExpOff()
      : PrefetchBrowserTestBase(true, false) {}
};

class PrefetchBrowserTestPredictionOffExpOn : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOffExpOn()
      : PrefetchBrowserTestBase(false, true) {}
};

class PrefetchBrowserTestPredictionOffExpOff : public PrefetchBrowserTestBase {
 public:
  PrefetchBrowserTestPredictionOffExpOff()
      : PrefetchBrowserTestBase(false, false) {}
};

// Privacy option is on, experiment is on.  Prefetch should succeed.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOn, PredOnExpOn) {
  EXPECT_TRUE(RunPrefetchExperiment(true, browser()));
}

// Privacy option is on, experiment is off.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOff, PredOnExpOff) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Privacy option is off, experiment is on.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOffExpOn, PredOffExpOn) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Privacy option is off, experiment is off.  Prefetch should be dropped.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOffExpOff, PredOffExpOff) {
  EXPECT_TRUE(RunPrefetchExperiment(false, browser()));
}

// Bug 339909: When in incognito mode the browser crashed due to an
// uninitialized preference member. Verify that it no longer does.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTestPredictionOnExpOn, IncognitoTest) {
  Profile* incognito_profile = browser()->profile()->GetOffTheRecordProfile();
  Browser* incognito_browser = new Browser(
      Browser::CreateParams(incognito_profile, browser()->host_desktop_type()));

  // Navigate just to have a tab in this window, otherwise there is no
  // WebContents for the incognito browser.
  ui_test_utils::OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));

  EXPECT_TRUE(RunPrefetchExperiment(true, incognito_browser));
}

}  // namespace

