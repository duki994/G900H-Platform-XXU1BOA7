// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_result_list_view.h"

#include <map>

#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/app_list/views/search_result_list_view_delegate.h"
#include "ui/views/test/views_test_base.h"

namespace app_list {
namespace test {

namespace {
int kDefaultSearchItems = 5;
}  // namespace

class SearchResultListViewTest : public views::ViewsTestBase,
                                 public SearchResultListViewDelegate {
 public:
  SearchResultListViewTest() {}
  virtual ~SearchResultListViewTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    view_.reset(new SearchResultListView(this, &view_delegate_));
    view_->SetResults(view_delegate_.GetModel()->results());
    view_->SetSelectedIndex(0);
  }

 protected:
  SearchResultListView* view() { return view_.get(); }

  void SetLongAutoLaunchTimeout() {
    // Sets a long timeout that lasts longer than the test run.
    view_delegate_.set_auto_launch_timeout(base::TimeDelta::FromDays(1));
  }

  base::TimeDelta GetAutoLaunchTimeout() {
    return view_delegate_.GetAutoLaunchTimeout();
  }

  void SetUpSearchResults() {
    AppListModel::SearchResults* results = view_delegate_.GetModel()->results();
    for (int i = 0; i < kDefaultSearchItems; ++i)
      results->Add(new SearchResult());

    // Adding results will schedule Update().
    RunPendingMessages();
  }

  int GetOpenResultCountAndReset(int ranking) {
    int result = open_result_counts_[ranking];
    open_result_counts_.clear();
    return result;
  }

  int GetSearchResults() {
    return view_->last_visible_index_ + 1;
  }

  int GetSelectedIndex() {
    return view_->selected_index_;
  }

  void ResetSelectedIndex() {
    view_->SetSelectedIndex(0);
  }

  bool KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE, true);
    return view_->OnKeyPressed(event);
  }

  bool IsAutoLaunching() {
    return view_->auto_launch_animation_;
  }

  void ForceAutoLaunch() {
    view_->ForceAutoLaunchForTest();
  }

 private:
  // Overridden from SearchResultListViewDelegate:
  virtual void OpenResult(SearchResult* result,
                          bool auto_launch,
                          int event_flags) OVERRIDE {
    const AppListModel::SearchResults* results =
        view_delegate_.GetModel()->results();
    for (size_t i = 0; i < results->item_count(); ++i) {
      if (results->GetItemAt(i) == result) {
        open_result_counts_[i]++;
        break;
      }
    }
  }
  virtual void InvokeResultAction(SearchResult* result,
                                  int action_index,
                                  int event_flags) OVERRIDE {}
  virtual void OnResultInstalled(SearchResult* result) OVERRIDE {}
  virtual void OnResultUninstalled(SearchResult* result) OVERRIDE {}

  AppListTestViewDelegate view_delegate_;
  scoped_ptr<SearchResultListView> view_;
  std::map<int, int> open_result_counts_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultListViewTest);
};

TEST_F(SearchResultListViewTest, Basic) {
  SetUpSearchResults();

  const int results = GetSearchResults();
  EXPECT_EQ(kDefaultSearchItems, results);
  EXPECT_EQ(0, GetSelectedIndex());
  EXPECT_FALSE(IsAutoLaunching());

  EXPECT_TRUE(KeyPress(ui::VKEY_RETURN));
  EXPECT_EQ(1, GetOpenResultCountAndReset(0));

  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
    EXPECT_EQ(i, GetSelectedIndex());
  }
  // Doesn't rotate.
  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_EQ(results - 1, GetSelectedIndex());

  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_UP));
    EXPECT_EQ(results - i - 1, GetSelectedIndex());
  }
  // Doesn't rotate.
  EXPECT_TRUE(KeyPress(ui::VKEY_UP));
  EXPECT_EQ(0, GetSelectedIndex());
  ResetSelectedIndex();

  for (int i = 1; i < results; ++i) {
    EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
    EXPECT_EQ(i, GetSelectedIndex());
  }
  // Doesn't rotate.
  EXPECT_TRUE(KeyPress(ui::VKEY_TAB));
  EXPECT_EQ(results - 1, GetSelectedIndex());
}

TEST_F(SearchResultListViewTest, AutoLaunch) {
  SetLongAutoLaunchTimeout();
  SetUpSearchResults();

  EXPECT_TRUE(IsAutoLaunching());
  ForceAutoLaunch();

  EXPECT_FALSE(IsAutoLaunching());
  EXPECT_EQ(1, GetOpenResultCountAndReset(0));

  // The timeout has to be cleared after the auto-launch, to prevent opening
  // the search result twice. See the comment in AnimationEnded().
  EXPECT_EQ(base::TimeDelta(), GetAutoLaunchTimeout());
}

TEST_F(SearchResultListViewTest, CancelAutoLaunch) {
  SetLongAutoLaunchTimeout();
  SetUpSearchResults();

  EXPECT_TRUE(IsAutoLaunching());

  EXPECT_TRUE(KeyPress(ui::VKEY_DOWN));
  EXPECT_FALSE(IsAutoLaunching());

  SetLongAutoLaunchTimeout();
  view()->UpdateAutoLaunchState();
  EXPECT_TRUE(IsAutoLaunching());

  view()->SetVisible(false);
  EXPECT_FALSE(IsAutoLaunching());

  SetLongAutoLaunchTimeout();
  view()->SetVisible(true);
  EXPECT_TRUE(IsAutoLaunching());
}

}  // namespace test
}  // namespace app_list
