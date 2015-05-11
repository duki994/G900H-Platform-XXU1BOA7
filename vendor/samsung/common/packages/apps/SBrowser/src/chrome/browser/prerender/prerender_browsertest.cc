// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/uma_histogram_helper.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "extensions/common/switches.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::TestNavigationObserver;
using content::WebContents;
using content::WebContentsObserver;

// Prerender tests work as follows:
//
// A page with a prefetch link to the test page is loaded.  Once prerendered,
// its Javascript function DidPrerenderPass() is called, which returns true if
// the page behaves as expected when prerendered.
//
// The prerendered page is then displayed on a tab.  The Javascript function
// DidDisplayPass() is called, and returns true if the page behaved as it
// should while being displayed.

namespace prerender {

namespace {

// Constants used in the test HTML files.
const char* kReadyTitle = "READY";
const char* kPassTitle = "PASS";

std::string CreateClientRedirect(const std::string& dest_url) {
  const char* const kClientRedirectBase = "client-redirect?";
  return kClientRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "server-redirect?";
  return kServerRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Browser* browser, int remove_mask) {
  BrowsingDataRemover* remover =
      BrowsingDataRemover::CreateForUnboundedRange(browser->profile());
  remover->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
  // BrowsingDataRemover deletes itself.
}

// Returns true if the prerender is expected to abort on its own, before
// attempting to swap it.
bool ShouldAbortPrerenderBeforeSwap(FinalStatus status) {
  switch (status) {
    case FINAL_STATUS_USED:
    case FINAL_STATUS_WINDOW_OPENER:
    case FINAL_STATUS_APP_TERMINATING:
    case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    // We'll crash the renderer after it's loaded.
    case FINAL_STATUS_RENDERER_CRASHED:
    case FINAL_STATUS_CANCELLED:
    case FINAL_STATUS_DEVTOOLS_ATTACHED:
    case FINAL_STATUS_PAGE_BEING_CAPTURED:
    case FINAL_STATUS_NAVIGATION_UNCOMMITTED:
    case FINAL_STATUS_WOULD_HAVE_BEEN_USED:
      return false;
    default:
      return true;
  }
}

// Convenience function to wait for a title. Handles the case when the
// WebContents already has the expected title.
void WaitForASCIITitle(WebContents* web_contents,
                       const char* expected_title_ascii) {
  base::string16 expected_title = base::ASCIIToUTF16(expected_title_ascii);
  if (web_contents->GetTitle() == expected_title)
    return;
  content::TitleWatcher title_watcher(web_contents, expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Waits for the destruction of a RenderProcessHost's IPC channel.
// Used to make sure the PrerenderLinkManager's OnChannelClosed function has
// been called, before checking its state.
class ChannelDestructionWatcher {
 public:
  ChannelDestructionWatcher() : channel_destroyed_(false) {
  }

  ~ChannelDestructionWatcher() {
  }

  void WatchChannel(content::RenderProcessHost* host) {
    host->AddFilter(new DestructionMessageFilter(this));
  }

  void WaitForChannelClose() {
    run_loop_.Run();
    EXPECT_TRUE(channel_destroyed_);
  }

 private:
  // When destroyed, calls ChannelDestructionWatcher::OnChannelDestroyed.
  // Ignores all messages.
  class DestructionMessageFilter : public content::BrowserMessageFilter {
   public:
     explicit DestructionMessageFilter(ChannelDestructionWatcher* watcher)
         : watcher_(watcher) {
    }

   private:
    virtual ~DestructionMessageFilter() {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&ChannelDestructionWatcher::OnChannelDestroyed,
                     base::Unretained(watcher_)));
    }

    virtual bool OnMessageReceived(const IPC::Message& message,
                                   bool* message_was_ok) OVERRIDE {
      return false;
    }

    ChannelDestructionWatcher* watcher_;

    DISALLOW_COPY_AND_ASSIGN(DestructionMessageFilter);
  };

  void OnChannelDestroyed() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    EXPECT_FALSE(channel_destroyed_);
    channel_destroyed_ = true;
    run_loop_.Quit();
  }

  bool channel_destroyed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDestructionWatcher);
};

// A navigation observer to wait on either a new load or a swap of a
// WebContents. On swap, if the new WebContents is still loading, wait for that
// load to complete as well. Note that the load must begin after the observer is
// attached.
class NavigationOrSwapObserver : public WebContentsObserver,
                                 public TabStripModelObserver {
 public:
  // Waits for either a new load or a swap of |tab_strip_model|'s active
  // WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(1) {
    CHECK_NE(TabStripModel::kNoTab,
             tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  // Waits for either |number_of_loads| loads or a swap of |tab_strip_model|'s
  // active WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents,
                           int number_of_loads)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(number_of_loads) {
    CHECK_NE(TabStripModel::kNoTab,
             tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  virtual ~NavigationOrSwapObserver() {
    tab_strip_model_->RemoveObserver(this);
  }

  void Wait() {
    loop_.Run();
  }

  // WebContentsObserver implementation:
  virtual void DidStartLoading(RenderViewHost* render_view_host) OVERRIDE {
    did_start_loading_ = true;
  }
  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE {
    if (!did_start_loading_)
      return;
    number_of_loads_--;
    if (number_of_loads_ == 0)
      loop_.Quit();
  }

  // TabStripModelObserver implementation:
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             WebContents* old_contents,
                             WebContents* new_contents,
                             int index) OVERRIDE {
    if (old_contents != web_contents())
      return;
    // Switch to observing the new WebContents.
    Observe(new_contents);
    if (new_contents->IsLoading()) {
      // If the new WebContents is still loading, wait for it to complete. Only
      // one load post-swap is supported.
      did_start_loading_ = true;
      number_of_loads_ = 1;
    } else {
      loop_.Quit();
    }
  }

 private:
  TabStripModel* tab_strip_model_;
  bool did_start_loading_;
  int number_of_loads_;
  base::RunLoop loop_;
};

// Waits for a new tab to open and a navigation or swap in it.
class NewTabNavigationOrSwapObserver {
 public:
  NewTabNavigationOrSwapObserver()
      : new_tab_observer_(
            chrome::NOTIFICATION_TAB_ADDED,
            base::Bind(&NewTabNavigationOrSwapObserver::OnTabAdded,
                       base::Unretained(this))) {
    // Watch for NOTIFICATION_TAB_ADDED. Add a callback so that the
    // NavigationOrSwapObserver can be attached synchronously and no events are
    // missed.
  }

  void Wait() {
    new_tab_observer_.Wait();
    swap_observer_->Wait();
  }

  bool OnTabAdded(const content::NotificationSource& source,
                  const content::NotificationDetails& details) {
    if (swap_observer_)
      return true;
    WebContents* new_tab = content::Details<WebContents>(details).ptr();
    // Get the TabStripModel. Assume this is attached to a Browser.
    TabStripModel* tab_strip_model =
        static_cast<Browser*>(new_tab->GetDelegate())->tab_strip_model();
    swap_observer_.reset(new NavigationOrSwapObserver(tab_strip_model,
                                                      new_tab));
    return true;
  }

 private:
  content::WindowedNotificationObserver new_tab_observer_;
  scoped_ptr<NavigationOrSwapObserver> swap_observer_;
};

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin,
      FinalStatus expected_final_status)
      : PrerenderContents(prerender_manager, profile, url,
                          referrer, origin, PrerenderManager::kNoExperiment),
        expected_final_status_(expected_final_status),
        new_render_view_host_(NULL),
        was_hidden_(false),
        was_shown_(false),
        should_be_shown_(expected_final_status == FINAL_STATUS_USED),
        skip_final_checks_(false) {
  }

  virtual ~TestPrerenderContents() {
    if (skip_final_checks_)
      return;

    if (expected_final_status_ == FINAL_STATUS_MAX) {
      EXPECT_EQ(MATCH_COMPLETE_REPLACEMENT, match_complete_status());
    } else {
      EXPECT_EQ(expected_final_status_, final_status()) <<
          " when testing URL " << prerender_url().path() <<
          " (Expected: " << NameFromFinalStatus(expected_final_status_) <<
          ", Actual: " << NameFromFinalStatus(final_status()) << ")";
    }
    // Prerendering RenderViewHosts should be hidden before the first
    // navigation, so this should be happen for every PrerenderContents for
    // which a RenderViewHost is created, regardless of whether or not it's
    // used.
    if (new_render_view_host_)
      EXPECT_TRUE(was_hidden_);

    // A used PrerenderContents will only be destroyed when we swap out
    // WebContents, at the end of a navigation caused by a call to
    // NavigateToURLImpl().
    if (final_status() == FINAL_STATUS_USED)
      EXPECT_TRUE(new_render_view_host_);

    EXPECT_EQ(should_be_shown_, was_shown_);
  }

  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE {
    // On quit, it's possible to end up here when render processes are closed
    // before the PrerenderManager is destroyed.  As a result, it's possible to
    // get either FINAL_STATUS_APP_TERMINATING or FINAL_STATUS_RENDERER_CRASHED
    // on quit.
    //
    // It's also possible for this to be called after we've been notified of
    // app termination, but before we've been deleted, which is why the second
    // check is needed.
    if (expected_final_status_ == FINAL_STATUS_APP_TERMINATING &&
        final_status() != expected_final_status_) {
      expected_final_status_ = FINAL_STATUS_RENDERER_CRASHED;
    }

    PrerenderContents::RenderProcessGone(status);
  }

  virtual bool CheckURL(const GURL& url) OVERRIDE {
    // Prevent FINAL_STATUS_UNSUPPORTED_SCHEME when navigating to about:crash in
    // the PrerenderRendererCrash test.
    if (url.spec() != content::kChromeUICrashURL)
      return PrerenderContents::CheckURL(url);
    return true;
  }

  // For tests that open the prerender in a new background tab, the RenderView
  // will not have been made visible when the PrerenderContents is destroyed
  // even though it is used.
  void set_should_be_shown(bool value) { should_be_shown_ = value; }

  // For tests which do not know whether the prerender will be used.
  void set_skip_final_checks(bool value) { skip_final_checks_ = value; }

  FinalStatus expected_final_status() const { return expected_final_status_; }

 private:
  virtual void OnRenderViewHostCreated(
      RenderViewHost* new_render_view_host) OVERRIDE {
    // Used to make sure the RenderViewHost is hidden and, if used,
    // subsequently shown.
    notification_registrar().Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(new_render_view_host));

    new_render_view_host_ = new_render_view_host;

    PrerenderContents::OnRenderViewHostCreated(new_render_view_host);
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type ==
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED) {
      EXPECT_EQ(new_render_view_host_,
                content::Source<RenderWidgetHost>(source).ptr());
      bool is_visible = *content::Details<bool>(details).ptr();

      if (!is_visible) {
        was_hidden_ = true;
      } else if (is_visible && was_hidden_) {
        // Once hidden, a prerendered RenderViewHost should only be shown after
        // being removed from the PrerenderContents for display.
        EXPECT_FALSE(GetRenderViewHost());
        was_shown_ = true;
      }
      return;
    }
    PrerenderContents::Observe(type, source, details);
  }

  FinalStatus expected_final_status_;

  // The RenderViewHost created for the prerender, if any.
  RenderViewHost* new_render_view_host_;
  // Set to true when the prerendering RenderWidget is hidden.
  bool was_hidden_;
  // Set to true when the prerendering RenderWidget is shown, after having been
  // hidden.
  bool was_shown_;
  // Expected final value of was_shown_.  Defaults to true for
  // FINAL_STATUS_USED, and false otherwise.
  bool should_be_shown_;
  // If true, |expected_final_status_| and other shutdown checks are skipped.
  bool skip_final_checks_;
};

// A handle to a TestPrerenderContents whose lifetime is under the caller's
// control. A PrerenderContents may be destroyed at any point. This allows
// tracking the final status, etc.
class TestPrerender : public PrerenderContents::Observer,
                      public base::SupportsWeakPtr<TestPrerender> {
 public:
  TestPrerender()
      : contents_(NULL),
        number_of_loads_(0),
        expected_number_of_loads_(0) {
  }
  virtual ~TestPrerender() {
    if (contents_)
      contents_->RemoveObserver(this);
  }

  TestPrerenderContents* contents() const { return contents_; }
  int number_of_loads() const { return number_of_loads_; }

  void WaitForCreate() { create_loop_.Run(); }
  void WaitForStart() { start_loop_.Run(); }
  void WaitForStop() { stop_loop_.Run(); }

  // Waits for |number_of_loads()| to be at least |expected_number_of_loads| OR
  // for the prerender to stop running (just to avoid a timeout if the prerender
  // dies). Note: this does not assert equality on the number of loads; the
  // caller must do it instead.
  void WaitForLoads(int expected_number_of_loads) {
    DCHECK(!load_waiter_);
    DCHECK(!expected_number_of_loads_);
    if (number_of_loads_ < expected_number_of_loads) {
      load_waiter_.reset(new base::RunLoop);
      expected_number_of_loads_ = expected_number_of_loads;
      load_waiter_->Run();
      load_waiter_.reset();
      expected_number_of_loads_ = 0;
    }
    EXPECT_LE(expected_number_of_loads, number_of_loads_);
  }

  void OnPrerenderCreated(TestPrerenderContents* contents) {
    DCHECK(!contents_);
    contents_ = contents;
    contents_->AddObserver(this);
    create_loop_.Quit();
  }

  // PrerenderContents::Observer implementation:
  virtual void OnPrerenderStart(PrerenderContents* contents) OVERRIDE {
    start_loop_.Quit();
  }

  virtual void OnPrerenderStopLoading(PrerenderContents* contents) OVERRIDE {
    number_of_loads_++;
    if (load_waiter_ && number_of_loads_ >= expected_number_of_loads_)
      load_waiter_->Quit();
  }

  virtual void OnPrerenderStop(PrerenderContents* contents) OVERRIDE {
    DCHECK(contents_);
    contents_ = NULL;
    stop_loop_.Quit();
    // If there is a WaitForLoads call and it has yet to see the expected number
    // of loads, stop the loop so the test fails instead of timing out.
    if (load_waiter_)
      load_waiter_->Quit();
  }

  virtual void OnPrerenderCreatedMatchCompleteReplacement(
      PrerenderContents* contents, PrerenderContents* replacement) OVERRIDE {
  }

 private:
  TestPrerenderContents* contents_;
  int number_of_loads_;

  int expected_number_of_loads_;
  scoped_ptr<base::RunLoop> load_waiter_;

  base::RunLoop create_loop_;
  base::RunLoop start_loop_;
  base::RunLoop stop_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestPrerender);
};

// PrerenderManager that uses TestPrerenderContents.
class TestPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  TestPrerenderContentsFactory() {}

  virtual ~TestPrerenderContentsFactory() {
    EXPECT_TRUE(expected_contents_queue_.empty());
  }

  scoped_ptr<TestPrerender> ExpectPrerenderContents(FinalStatus final_status) {
    scoped_ptr<TestPrerender> handle(new TestPrerender());
    expected_contents_queue_.push_back(
        ExpectedContents(final_status, handle->AsWeakPtr()));
    return handle.Pass();
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin,
      uint8 experiment_id) OVERRIDE {
    ExpectedContents expected;
    if (!expected_contents_queue_.empty()) {
      expected = expected_contents_queue_.front();
      expected_contents_queue_.pop_front();
    }
    VLOG(1) << "Creating prerender contents for " << url.path() <<
               " with expected final status " << expected.final_status;
    VLOG(1) << expected_contents_queue_.size() << " left in the queue.";
    TestPrerenderContents* contents =
        new TestPrerenderContents(prerender_manager,
                                  profile, url, referrer, origin,
                                  expected.final_status);
    if (expected.handle)
      expected.handle->OnPrerenderCreated(contents);
    return contents;
  }

 private:
  struct ExpectedContents {
    ExpectedContents() : final_status(FINAL_STATUS_MAX) { }
    ExpectedContents(FinalStatus final_status,
                     const base::WeakPtr<TestPrerender>& handle)
        : final_status(final_status),
          handle(handle) {
    }

    FinalStatus final_status;
    base::WeakPtr<TestPrerender> handle;
  };

  std::deque<ExpectedContents> expected_contents_queue_;
};

#if defined(FULL_SAFE_BROWSING)
// A SafeBrowsingDatabaseManager implementation that returns a fixed result for
// a given URL.
class FakeSafeBrowsingDatabaseManager :  public SafeBrowsingDatabaseManager {
 public:
  explicit FakeSafeBrowsingDatabaseManager(SafeBrowsingService* service)
      : SafeBrowsingDatabaseManager(service),
        threat_type_(SB_THREAT_TYPE_SAFE) { }

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Returns true, indicating a SAFE result, unless the URL is the fixed URL
  // specified by the user, and the user-specified result is not SAFE
  // (in which that result will be communicated back via a call into the
  // client, and false will be returned).
  // Overrides SafeBrowsingService::CheckBrowseUrl.
  virtual bool CheckBrowseUrl(const GURL& gurl, Client* client) OVERRIDE {
    if (gurl != url_ || threat_type_ == SB_THREAT_TYPE_SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&FakeSafeBrowsingDatabaseManager::OnCheckBrowseURLDone,
                   this, gurl, client));
    return false;
  }

  void SetThreatTypeForUrl(const GURL& url, SBThreatType threat_type) {
    url_ = url;
    threat_type_ = threat_type;
  }

 private:
  virtual ~FakeSafeBrowsingDatabaseManager() {}

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    std::vector<SBThreatType> expected_threats;
    expected_threats.push_back(SB_THREAT_TYPE_URL_MALWARE);
    expected_threats.push_back(SB_THREAT_TYPE_URL_PHISHING);
    SafeBrowsingDatabaseManager::SafeBrowsingCheck sb_check(
        std::vector<GURL>(1, gurl),
        std::vector<SBFullHash>(),
        client,
        safe_browsing_util::MALWARE,
        expected_threats);
    sb_check.url_results[0] = threat_type_;
    client->OnSafeBrowsingResult(sb_check);
  }

  GURL url_;
  SBThreatType threat_type_;
  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingDatabaseManager);
};

class FakeSafeBrowsingService : public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() { }

  // Returned pointer has the same lifespan as the database_manager_ refcounted
  // object.
  FakeSafeBrowsingDatabaseManager* fake_database_manager() {
    return fake_database_manager_;
  }

 protected:
  virtual ~FakeSafeBrowsingService() { }

  virtual SafeBrowsingDatabaseManager* CreateDatabaseManager() OVERRIDE {
    fake_database_manager_ = new FakeSafeBrowsingDatabaseManager(this);
    return fake_database_manager_;
  }

 private:
  FakeSafeBrowsingDatabaseManager* fake_database_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeSafeBrowsingService);
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() :
      most_recent_service_(NULL) { }
  virtual ~TestSafeBrowsingServiceFactory() { }

  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    most_recent_service_ =  new FakeSafeBrowsingService();
    return most_recent_service_;
  }

  FakeSafeBrowsingService* most_recent_service() const {
    return most_recent_service_;
  }

 private:
  FakeSafeBrowsingService* most_recent_service_;
};
#endif

class FakeDevToolsClientHost : public DevToolsClientHost {
 public:
  FakeDevToolsClientHost() {}
  virtual ~FakeDevToolsClientHost() {}
  virtual void InspectedContentsClosing() OVERRIDE {}
  virtual void DispatchOnInspectorFrontend(const std::string& msg) OVERRIDE {}
  virtual void ReplacedWithAnotherClient() OVERRIDE {}
};

class RestorePrerenderMode {
 public:
  RestorePrerenderMode() : prev_mode_(PrerenderManager::GetMode()) {
  }

  ~RestorePrerenderMode() { PrerenderManager::SetMode(prev_mode_); }
 private:
  PrerenderManager::PrerenderManagerMode prev_mode_;
};

// URLRequestJob (and associated handler) which hangs.
class HangingURLRequestJob : public net::URLRequestJob {
 public:
  HangingURLRequestJob(net::URLRequest* request,
                          net::NetworkDelegate* network_delegate)
      : net::URLRequestJob(request, network_delegate) {
  }

  virtual void Start() OVERRIDE {}

 private:
  virtual ~HangingURLRequestJob() {}
};

class HangingFirstRequestProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  HangingFirstRequestProtocolHandler(const base::FilePath& file,
                                     base::Closure callback)
      : file_(file),
        callback_(callback),
        first_run_(true) {
  }
  virtual ~HangingFirstRequestProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    if (first_run_) {
      first_run_ = false;
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, callback_);
      return new HangingURLRequestJob(request, network_delegate);
    }
    return new content::URLRequestMockHTTPJob(request, network_delegate, file_);
  }

 private:
  base::FilePath file_;
  base::Closure callback_;
  mutable bool first_run_;
};

// Makes |url| never respond on the first load, and then with the contents of
// |file| afterwards. When the first load has been scheduled, runs |callback| on
// the UI thread.
void CreateHangingFirstRequestProtocolHandlerOnIO(const GURL& url,
                                                  const base::FilePath& file,
                                                  base::Closure callback) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler> never_respond_handler(
      new HangingFirstRequestProtocolHandler(file, callback));
  net::URLRequestFilter::GetInstance()->AddUrlProtocolHandler(
      url, never_respond_handler.Pass());
}

// Wrapper over URLRequestMockHTTPJob that exposes extra callbacks.
class MockHTTPJob : public content::URLRequestMockHTTPJob {
 public:
  MockHTTPJob(net::URLRequest* request,
              net::NetworkDelegate* delegate,
              const base::FilePath& file)
      : content::URLRequestMockHTTPJob(request, delegate, file) {
  }

  void set_start_callback(const base::Closure& start_callback) {
    start_callback_ = start_callback;
  }

  virtual void Start() OVERRIDE {
    if (!start_callback_.is_null())
      start_callback_.Run();
    content::URLRequestMockHTTPJob::Start();
  }

 private:
  virtual ~MockHTTPJob() {}

  base::Closure start_callback_;
};

// Dummy counter class to live on the UI thread for counting requests.
class RequestCounter : public base::SupportsWeakPtr<RequestCounter> {
 public:
  RequestCounter() : count_(0), expected_count_(-1) {}
  int count() const { return count_; }

  void RequestStarted() {
    count_++;
    if (loop_ && count_ == expected_count_)
      loop_->Quit();
  }

  void WaitForCount(int expected_count) {
    ASSERT_TRUE(!loop_);
    ASSERT_EQ(-1, expected_count_);
    if (count_ < expected_count) {
      expected_count_ = expected_count;
      loop_.reset(new base::RunLoop);
      loop_->Run();
      expected_count_ = -1;
      loop_.reset();
    }

    EXPECT_EQ(expected_count, count_);
  }
 private:
  int count_;
  int expected_count_;
  scoped_ptr<base::RunLoop> loop_;
};

// Protocol handler which counts the number of requests that start.
class CountingProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  CountingProtocolHandler(const base::FilePath& file,
                          const base::WeakPtr<RequestCounter>& counter)
      : file_(file),
        counter_(counter),
        weak_factory_(this) {
  }
  virtual ~CountingProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    MockHTTPJob* job = new MockHTTPJob(request, network_delegate, file_);
    job->set_start_callback(base::Bind(&CountingProtocolHandler::RequestStarted,
                                       weak_factory_.GetWeakPtr()));
    return job;
  }

  void RequestStarted() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&RequestCounter::RequestStarted, counter_));
  }

 private:
  base::FilePath file_;
  base::WeakPtr<RequestCounter> counter_;
  mutable base::WeakPtrFactory<CountingProtocolHandler> weak_factory_;
};

// Makes |url| respond to requests with the contents of |file|, counting the
// number that start in |counter|.
void CreateCountingProtocolHandlerOnIO(
    const GURL& url,
    const base::FilePath& file,
    const base::WeakPtr<RequestCounter>& counter) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  scoped_ptr<net::URLRequestJobFactory::ProtocolHandler> protocol_handler(
      new CountingProtocolHandler(file, counter));
  net::URLRequestFilter::GetInstance()->AddUrlProtocolHandler(
      url, protocol_handler.Pass());
}

// Makes |url| respond to requests with the contents of |file|.
void CreateMockProtocolHandlerOnIO(const GURL& url,
                                   const base::FilePath& file) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestFilter::GetInstance()->AddUrlProtocolHandler(
      url, content::URLRequestMockHTTPJob::CreateProtocolHandlerForSingleFile(
          file));
}

// A ContentBrowserClient that cancels all prerenderers on OpenURL.
class TestContentBrowserClient : public chrome::ChromeContentBrowserClient {
 public:
  TestContentBrowserClient() {}
  virtual ~TestContentBrowserClient() {}

  // chrome::ChromeContentBrowserClient implementation.
  virtual bool ShouldAllowOpenURL(content::SiteInstance* site_instance,
                                  const GURL& url) OVERRIDE {
    PrerenderManagerFactory::GetForProfile(
        Profile::FromBrowserContext(site_instance->GetBrowserContext()))
        ->CancelAllPrerenders();
    return chrome::ChromeContentBrowserClient::ShouldAllowOpenURL(site_instance,
                                                                  url);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestContentBrowserClient);
};

// A ContentBrowserClient that forces cross-process navigations.
class SwapProcessesContentBrowserClient
    : public chrome::ChromeContentBrowserClient {
 public:
  SwapProcessesContentBrowserClient() {}
  virtual ~SwapProcessesContentBrowserClient() {}

  // chrome::ChromeContentBrowserClient implementation.
  virtual bool ShouldSwapProcessesForRedirect(
      content::ResourceContext* resource_context,
      const GURL& current_url,
      const GURL& new_url) OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SwapProcessesContentBrowserClient);
};

// An ExternalProtocolHandler that blocks everything and asserts it never is
// called.
class NeverRunsExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  // ExternalProtocolHandler::Delegate implementation.
  virtual ShellIntegration::DefaultProtocolClientWorker* CreateShellWorker(
      ShellIntegration::DefaultWebClientObserver* observer,
      const std::string& protocol) OVERRIDE {
    NOTREACHED();
    // This will crash, but it shouldn't get this far with BlockState::BLOCK
    // anyway.
    return NULL;
  }
  virtual ExternalProtocolHandler::BlockState GetBlockState(
      const std::string& scheme) OVERRIDE {
    // Block everything and fail the test.
    ADD_FAILURE();
    return ExternalProtocolHandler::BLOCK;
  }
  virtual void BlockRequest() OVERRIDE { }
  virtual void RunExternalProtocolDialog(const GURL& url,
                                         int render_process_host_id,
                                         int routing_id) OVERRIDE {
    NOTREACHED();
  }
  virtual void LaunchUrlWithoutSecurityCheck(const GURL& url) OVERRIDE {
    NOTREACHED();
  }
  virtual void FinishedProcessingCheck() OVERRIDE {
    NOTREACHED();
  }
};

base::FilePath GetTestPath(const std::string& file_name) {
  return ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("prerender")),
      base::FilePath().AppendASCII(file_name));
}

}  // namespace

// Many of these tests are flaky. See http://crbug.com/249179
class PrerenderBrowserTest : virtual public InProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : autostart_test_server_(true),
        prerender_contents_factory_(NULL),
#if defined(FULL_SAFE_BROWSING)
        safe_browsing_factory_(new TestSafeBrowsingServiceFactory()),
#endif
        call_javascript_(true),
        check_load_events_(true),
        loader_path_("files/prerender/prerender_loader.html"),
        explicitly_set_browser_(NULL) {}

  virtual ~PrerenderBrowserTest() {}

  content::SessionStorageNamespace* GetSessionStorageNamespace() const {
    WebContents* web_contents = GetActiveWebContents();
    if (!web_contents)
      return NULL;
    return web_contents->GetController().GetDefaultSessionStorageNamespace();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
#if defined(FULL_SAFE_BROWSING)
    SafeBrowsingService::RegisterFactory(safe_browsing_factory_.get());
#endif
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
#if defined(FULL_SAFE_BROWSING)
    SafeBrowsingService::RegisterFactory(NULL);
#endif
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kPrerenderMode,
                                    switches::kPrerenderModeSwitchValueEnabled);
#if defined(OS_MACOSX)
    // The plugins directory isn't read by default on the Mac, so it needs to be
    // explicitly registered.
    base::FilePath app_dir;
    PathService::Get(chrome::DIR_APP, &app_dir);
    command_line->AppendSwitchPath(
        switches::kExtraPluginDir,
        app_dir.Append(FILE_PATH_LITERAL("plugins")));
#endif
    command_line->AppendSwitch(switches::kAlwaysAuthorizePlugins);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    current_browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPromptForDownload, false);
    IncreasePrerenderMemory();
    if (autostart_test_server_)
      ASSERT_TRUE(test_server()->Start());
    ChromeResourceDispatcherHostDelegate::
        SetExternalProtocolHandlerDelegateForTesting(
            &external_protocol_handler_delegate_);

    PrerenderManager* prerender_manager = GetPrerenderManager();
    ASSERT_TRUE(prerender_manager);
    prerender_manager->mutable_config().rate_limit_enabled = false;
    ASSERT_TRUE(prerender_contents_factory_ == NULL);
    prerender_contents_factory_ = new TestPrerenderContentsFactory;
    prerender_manager->SetPrerenderContentsFactory(prerender_contents_factory_);
  }

  // Convenience function to get the currently active WebContents in
  // current_browser().
  WebContents* GetActiveWebContents() const {
    return current_browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Overload for a single expected final status
  scoped_ptr<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    GURL url = test_server()->GetURL(html_file);
    return PrerenderTestURL(url,
                            expected_final_status,
                            expected_number_of_loads);
  }

  ScopedVector<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    GURL url = test_server()->GetURL(html_file);
    return PrerenderTestURLImpl(url,
                                expected_final_status_queue,
                                expected_number_of_loads);
  }

  scoped_ptr<TestPrerender> PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    std::vector<FinalStatus> expected_final_status_queue(
        1, expected_final_status);
    std::vector<TestPrerender*> prerenders;
    PrerenderTestURLImpl(url,
                         expected_final_status_queue,
                         expected_number_of_loads).release(&prerenders);
    CHECK_EQ(1u, prerenders.size());
    return scoped_ptr<TestPrerender>(prerenders[0]);
  }

  void NavigateToDestURL() const {
    NavigateToDestURLWithDisposition(CURRENT_TAB, true);
  }

  // Opens the url in a new tab, with no opener.
  void NavigateToDestURLWithDisposition(
      WindowOpenDisposition disposition,
      bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url_, Referrer(), disposition,
                               content::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURL(const std::string& dest_html_file) const {
    NavigateToURLWithDisposition(dest_html_file, CURRENT_TAB, true);
  }

  void NavigateToURLWithDisposition(const std::string& dest_html_file,
                                    WindowOpenDisposition disposition,
                                    bool expect_swap_to_succeed) const {
    GURL dest_url = test_server()->GetURL(dest_html_file);
    NavigateToURLWithDisposition(dest_url, disposition, expect_swap_to_succeed);
  }

  void NavigateToURLWithDisposition(const GURL& dest_url,
                                    WindowOpenDisposition disposition,
                                    bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url, Referrer(), disposition,
                               content::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURLWithParams(const content::OpenURLParams& params,
                               bool expect_swap_to_succeed) const {
    NavigateToURLImpl(params, expect_swap_to_succeed);
  }

  void OpenDestURLViaClick() const {
    OpenURLViaClick(dest_url_);
  }

  void OpenURLViaClick(const GURL& url) const {
    OpenURLWithJSImpl("Click", url, GURL(), false);
  }

  void OpenDestURLViaClickTarget() const {
    OpenURLWithJSImpl("ClickTarget", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickPing(const GURL& ping_url) const {
    OpenURLWithJSImpl("ClickPing", dest_url_, ping_url, false);
  }

  void OpenDestURLViaClickNewWindow() const {
    OpenURLWithJSImpl("ShiftClick", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickNewForegroundTab() const {
#if defined(OS_MACOSX)
    OpenURLWithJSImpl("MetaShiftClick", dest_url_, GURL(), true);
#else
    OpenURLWithJSImpl("CtrlShiftClick", dest_url_, GURL(), true);
#endif
  }

  void OpenDestURLViaClickNewBackgroundTab() const {
#if defined(OS_MACOSX)
    OpenURLWithJSImpl("MetaClick", dest_url_, GURL(), true);
#else
    OpenURLWithJSImpl("CtrlClick", dest_url_, GURL(), true);
#endif
  }

  void OpenDestURLViaWindowOpen() const {
    OpenURLWithJSImpl("WindowOpen", dest_url_, GURL(), true);
  }

  void RemoveLinkElement(int i) const {
    GetActiveWebContents()->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
            base::string16(),
            base::ASCIIToUTF16(base::StringPrintf("RemoveLinkElement(%d)", i)));
  }

  void ClickToNextPageAfterPrerender() {
    TestNavigationObserver nav_observer(GetActiveWebContents());
    RenderViewHost* render_view_host =
        GetActiveWebContents()->GetRenderViewHost();
    render_view_host->ExecuteJavascriptInWebFrame(
        base::string16(),
        base::ASCIIToUTF16("ClickOpenLink()"));
    nav_observer.Wait();
  }

  void NavigateToNextPageAfterPrerender() const {
    ui_test_utils::NavigateToURL(
        current_browser(),
        test_server()->GetURL("files/prerender/prerender_page.html"));
  }

  // Called after the prerendered page has been navigated to and then away from.
  // Navigates back through the history to the prerendered page.
  void GoBackToPrerender() {
    TestNavigationObserver back_nav_observer(GetActiveWebContents());
    chrome::GoBack(current_browser(), CURRENT_TAB);
    back_nav_observer.Wait();
    bool original_prerender_page = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(IsOriginalPrerenderPage())",
        &original_prerender_page));
    EXPECT_TRUE(original_prerender_page);
  }

  // Goes back to the page that was active before the prerender was swapped
  // in. This must be called when the prerendered page is the current page
  // in the active tab.
  void GoBackToPageBeforePrerender() {
    WebContents* tab = GetActiveWebContents();
    ASSERT_TRUE(tab);
    EXPECT_FALSE(tab->IsLoading());
    TestNavigationObserver back_nav_observer(tab);
    chrome::GoBack(current_browser(), CURRENT_TAB);
    back_nav_observer.Wait();
    bool js_result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        tab,
        "window.domAutomationController.send(DidBackToOriginalPagePass())",
        &js_result));
    EXPECT_TRUE(js_result);
  }

  bool UrlIsInPrerenderManager(const std::string& html_file) const {
    return UrlIsInPrerenderManager(test_server()->GetURL(html_file));
  }

  bool UrlIsInPrerenderManager(const GURL& url) const {
    return GetPrerenderManager()->FindPrerenderData(
        url, GetSessionStorageNamespace()) != NULL;
  }

  void UseHttpsSrcServer() {
    if (https_src_server_)
      return;
    https_src_server_.reset(
        new net::SpawnedTestServer(
            net::SpawnedTestServer::TYPE_HTTPS,
            net::SpawnedTestServer::kLocalhost,
            base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))));
    CHECK(https_src_server_->Start());
  }

  void DisableJavascriptCalls() {
    call_javascript_ = false;
  }

  void DisableLoadEventCheck() {
    check_load_events_ = false;
  }

  TaskManagerModel* GetModel() const {
    return TaskManager::GetInstance()->model();
  }

  PrerenderManager* GetPrerenderManager() const {
    PrerenderManager* prerender_manager =
        PrerenderManagerFactory::GetForProfile(current_browser()->profile());
    return prerender_manager;
  }

  const PrerenderLinkManager* GetPrerenderLinkManager() const {
    PrerenderLinkManager* prerender_link_manager =
        PrerenderLinkManagerFactory::GetForProfile(
            current_browser()->profile());
    return prerender_link_manager;
  }

  bool DidReceivePrerenderStartEventForLinkNumber(int index) const {
    bool received_prerender_started;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send(Boolean("
            "receivedPrerenderStartEvents[%d]))", index);

    CHECK(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        expression,
        &received_prerender_started));
    return received_prerender_started;
  }

  int GetPrerenderLoadEventCountForLinkNumber(int index) const {
    int load_event_count;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send("
            "receivedPrerenderLoadEvents[%d] || 0)", index);

    CHECK(content::ExecuteScriptAndExtractInt(
        GetActiveWebContents(),
        expression,
        &load_event_count));
    return load_event_count;
  }

  int GetPrerenderDomContentLoadedEventCountForLinkNumber(int index) const {
    int dom_content_loaded_event_count;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send("
            "receivedPrerenderDomContentLoadedEvents[%d] || 0)", index);

    CHECK(content::ExecuteScriptAndExtractInt(
        GetActiveWebContents(),
        expression,
        &dom_content_loaded_event_count));
    return dom_content_loaded_event_count;
  }

  bool DidReceivePrerenderStopEventForLinkNumber(int index) const {
    bool received_prerender_stopped;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send(Boolean("
            "receivedPrerenderStopEvents[%d]))", index);

    CHECK(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        expression,
        &received_prerender_stopped));
    return received_prerender_stopped;
  }

  bool HadPrerenderEventErrors() const {
    bool had_prerender_event_errors;
    CHECK(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(Boolean("
        "    hadPrerenderEventErrors))",
        &had_prerender_event_errors));
    return had_prerender_event_errors;
  }

  // Asserting on this can result in flaky tests.  PrerenderHandles are
  // removed from the PrerenderLinkManager when the prerender is canceled from
  // the browser, when the prerenders are cancelled from the renderer process,
  // or the channel for the renderer process is closed on the IO thread.  In the
  // last case, the code must be careful to wait for the channel to close, as it
  // is done asynchronously after swapping out the old process.  See
  // ChannelDestructionWatcher.
  bool IsEmptyPrerenderLinkManager() const {
    return GetPrerenderLinkManager()->IsEmpty();
  }

  size_t GetLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->prerenders_.size();
  }

  size_t GetRunningLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->CountRunningPrerenders();
  }

  // Returns length of |prerender_manager_|'s history, or -1 on failure.
  int GetHistoryLength() const {
    scoped_ptr<base::DictionaryValue> prerender_dict(
        static_cast<base::DictionaryValue*>(
            GetPrerenderManager()->GetAsValue()));
    if (!prerender_dict.get())
      return -1;
    base::ListValue* history_list;
    if (!prerender_dict->GetList("history", &history_list))
      return -1;
    return static_cast<int>(history_list->GetSize());
  }

#if defined(FULL_SAFE_BROWSING)
  FakeSafeBrowsingDatabaseManager* GetFakeSafeBrowsingDatabaseManager() {
    return safe_browsing_factory_->most_recent_service()->
        fake_database_manager();
  }
#endif

  TestPrerenderContents* GetPrerenderContentsFor(const GURL& url) const {
    PrerenderManager::PrerenderData* prerender_data =
        GetPrerenderManager()->FindPrerenderData(url, NULL);
    return static_cast<TestPrerenderContents*>(
        prerender_data ? prerender_data->contents() : NULL);
  }

  void SetLoaderHostOverride(const std::string& host) {
    loader_host_override_ = host;
    host_resolver()->AddRule(host, "127.0.0.1");
  }

  void set_loader_path(const std::string& path) {
    loader_path_ = path;
  }

  void set_loader_query_and_fragment(const std::string& query_and_fragment) {
    loader_query_and_fragment_ = query_and_fragment;
  }

  GURL GetCrossDomainTestUrl(const std::string& path) {
    static const std::string secondary_domain = "www.foo.com";
    host_resolver()->AddRule(secondary_domain, "127.0.0.1");
    std::string url_str(base::StringPrintf(
        "http://%s:%d/%s",
        secondary_domain.c_str(),
        test_server()->host_port_pair().port(),
        path.c_str()));
    return GURL(url_str);
  }

  void set_browser(Browser* browser) {
    explicitly_set_browser_ = browser;
  }

  Browser* current_browser() const {
    return explicitly_set_browser_ ? explicitly_set_browser_ : browser();
  }

  const GURL& dest_url() const {
    return dest_url_;
  }

  void IncreasePrerenderMemory() {
    // Increase the memory allowed in a prerendered page above normal settings.
    // Debug build bots occasionally run against the default limit, and tests
    // were failing because the prerender was canceled due to memory exhaustion.
    // http://crbug.com/93076
    GetPrerenderManager()->mutable_config().max_bytes = 1000 * 1024 * 1024;
  }

  bool DidPrerenderPass(WebContents* web_contents) const {
    bool prerender_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidPrerenderPass())",
            &prerender_test_result))
      return false;
    return prerender_test_result;
  }

  bool DidDisplayPass(WebContents* web_contents) const {
    bool display_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidDisplayPass())",
            &display_test_result))
      return false;
    return display_test_result;
  }

  scoped_ptr<TestPrerender> ExpectPrerender(FinalStatus expected_final_status) {
    return prerender_contents_factory_->ExpectPrerenderContents(
        expected_final_status);
  }

  void AddPrerender(const GURL& url, int index) {
    std::string javascript = base::StringPrintf(
        "AddPrerender('%s', %d)", url.spec().c_str(), index);
    RenderViewHost* render_view_host =
        GetActiveWebContents()->GetRenderViewHost();
    render_view_host->ExecuteJavascriptInWebFrame(
        base::string16(), base::ASCIIToUTF16(javascript));
  }

 protected:
  bool autostart_test_server_;

 private:
  // TODO(davidben): Remove this altogether so the tests don't globally assume
  // only one prerender.
  TestPrerenderContents* GetPrerenderContents() const {
    return GetPrerenderContentsFor(dest_url_);
  }

  ScopedVector<TestPrerender> PrerenderTestURLImpl(
      const GURL& prerender_url,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    dest_url_ = prerender_url;

    std::vector<net::SpawnedTestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_PRERENDER_URL", prerender_url.spec()));
    std::string replacement_path;
    CHECK(net::SpawnedTestServer::GetFilePathWithReplacements(
        loader_path_,
        replacement_text,
        &replacement_path));

    const net::SpawnedTestServer* src_server = test_server();
    if (https_src_server_)
      src_server = https_src_server_.get();
    GURL loader_url = src_server->GetURL(replacement_path +
                                         loader_query_and_fragment_);

    GURL::Replacements loader_replacements;
    if (!loader_host_override_.empty())
      loader_replacements.SetHostStr(loader_host_override_);
    loader_url = loader_url.ReplaceComponents(loader_replacements);

    VLOG(1) << "Running test with queue length " <<
               expected_final_status_queue.size();
    CHECK(!expected_final_status_queue.empty());
    ScopedVector<TestPrerender> prerenders;
    for (size_t i = 0; i < expected_final_status_queue.size(); i++) {
      prerenders.push_back(
          prerender_contents_factory_->ExpectPrerenderContents(
              expected_final_status_queue[i]).release());
    }

    FinalStatus expected_final_status = expected_final_status_queue.front();

    // Navigate to the loader URL and then wait for the first prerender to be
    // created.
    ui_test_utils::NavigateToURL(current_browser(), loader_url);
    prerenders[0]->WaitForCreate();
    prerenders[0]->WaitForLoads(expected_number_of_loads);

    if (ShouldAbortPrerenderBeforeSwap(expected_final_status)) {
      // The prerender will abort on its own. Assert it does so correctly.
      prerenders[0]->WaitForStop();
      EXPECT_FALSE(prerenders[0]->contents());
      EXPECT_TRUE(DidReceivePrerenderStopEventForLinkNumber(0));
    } else {
      // Otherwise, check that it prerendered correctly.
      TestPrerenderContents* prerender_contents = prerenders[0]->contents();

      CHECK_NE(static_cast<PrerenderContents*>(NULL), prerender_contents);
      EXPECT_EQ(FINAL_STATUS_MAX, prerender_contents->final_status());
      EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));

      if (call_javascript_) {
        // Check if page behaves as expected while in prerendered state.
        EXPECT_TRUE(DidPrerenderPass(prerender_contents->prerender_contents()));
      }
    }

    // Test that the referring page received the right start and load events.
    EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
    if (check_load_events_) {
      EXPECT_EQ(expected_number_of_loads, prerenders[0]->number_of_loads());
      EXPECT_EQ(expected_number_of_loads,
                GetPrerenderLoadEventCountForLinkNumber(0));
    }
    EXPECT_FALSE(HadPrerenderEventErrors());

    return prerenders.Pass();
  }

  void NavigateToURLImpl(const content::OpenURLParams& params,
                         bool expect_swap_to_succeed) const {
    ASSERT_NE(static_cast<PrerenderManager*>(NULL), GetPrerenderManager());
    // Make sure in navigating we have a URL to use in the PrerenderManager.
    ASSERT_NE(static_cast<PrerenderContents*>(NULL), GetPrerenderContents());

    WebContents* web_contents = GetPrerenderContents()->prerender_contents();

    // Navigate and wait for either the load to finish normally or for a swap to
    // occur.
    // TODO(davidben): The only handles CURRENT_TAB navigations, which is the
    // only case tested or prerendered right now.
    CHECK_EQ(CURRENT_TAB, params.disposition);
    NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                           GetActiveWebContents());
    WebContents* target_web_contents = current_browser()->OpenURL(params);
    swap_observer.Wait();

    if (web_contents && expect_swap_to_succeed) {
      EXPECT_EQ(web_contents, target_web_contents);
      if (call_javascript_)
        EXPECT_TRUE(DidDisplayPass(web_contents));
    }
  }

  // Opens the prerendered page using javascript functions in the loader
  // page. |javascript_function_name| should be a 0 argument function which is
  // invoked. |new_web_contents| is true if the navigation is expected to
  // happen in a new WebContents via OpenURL.
  void OpenURLWithJSImpl(const std::string& javascript_function_name,
                         const GURL& url,
                         const GURL& ping_url,
                         bool new_web_contents) const {
    WebContents* web_contents = GetActiveWebContents();
    RenderViewHost* render_view_host =
        GetActiveWebContents()->GetRenderViewHost();
    // Extra arguments in JS are ignored.
    std::string javascript = base::StringPrintf(
        "%s('%s', '%s')", javascript_function_name.c_str(),
        url.spec().c_str(), ping_url.spec().c_str());

    if (new_web_contents) {
      NewTabNavigationOrSwapObserver observer;
      render_view_host->ExecuteJavascriptInWebFrame(
          base::string16(), base::ASCIIToUTF16(javascript));
      observer.Wait();
    } else {
      NavigationOrSwapObserver observer(current_browser()->tab_strip_model(),
                                        web_contents);
      render_view_host->ExecuteJavascriptInWebFrame(
          base::string16(), base::ASCIIToUTF16(javascript));
      observer.Wait();
    }
  }

  TestPrerenderContentsFactory* prerender_contents_factory_;
#if defined(FULL_SAFE_BROWSING)
  scoped_ptr<TestSafeBrowsingServiceFactory> safe_browsing_factory_;
#endif
  NeverRunsExternalProtocolHandlerDelegate external_protocol_handler_delegate_;
  GURL dest_url_;
  scoped_ptr<net::SpawnedTestServer> https_src_server_;
  bool call_javascript_;
  bool check_load_events_;
  std::string loader_host_override_;
  std::string loader_path_;
  std::string loader_query_and_fragment_;
  Browser* explicitly_set_browser_;
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prerender> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  UMAHistogramHelper histograms;

  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  EXPECT_EQ(1, GetPrerenderDomContentLoadedEventCountForLinkNumber(0));
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  histograms.ExpectTotalCount("Prerender.websame_PrerenderNotSwappedInPLT", 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLTMatched", 1);
  histograms.ExpectTotalCount(
      "Prerender.websame_PerceivedPLTMatchedComplete", 1);

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that cross-domain prerenders emit the correct histograms.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageCrossDomain) {
  UMAHistogramHelper histograms;

  PrerenderTestURL(GetCrossDomainTestUrl("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED, 1);
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  histograms.ExpectTotalCount("Prerender.webcross_PrerenderNotSwappedInPLT", 1);

  NavigateToDestURL();
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.webcross_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.webcross_PerceivedPLTMatched", 1);
  histograms.ExpectTotalCount(
      "Prerender.webcross_PerceivedPLTMatchedComplete", 1);
}

// Checks that pending prerenders launch and receive proper event treatment.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPagePending) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page_pending.html",
                       FINAL_STATUS_USED, 1);

  // Navigate to the prerender.
  scoped_ptr<TestPrerender> prerender2 = ExpectPrerender(FINAL_STATUS_USED);
  NavigateToDestURL();
  // Abort early if the original prerender didn't swap, so as not to hang.
  ASSERT_FALSE(prerender->contents());

  // Wait for the new prerender to be ready.
  prerender2->WaitForStart();
  prerender2->WaitForLoads(1);

  const GURL prerender_page_url =
      test_server()->GetURL("files/prerender/prerender_page.html");
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  EXPECT_NE(static_cast<TestPrerenderContents*>(NULL),
            GetPrerenderContentsFor(prerender_page_url));

  // Now navigate to our target page.
  NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                         GetActiveWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), prerender_page_url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  swap_observer.Wait();

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that pending prerenders which are canceled before they are launched
// never get started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageRemovesPending) {
  PrerenderTestURL("files/prerender/prerender_page_removes_pending.html",
                   FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  EXPECT_FALSE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// Flaky, http://crbug.com/167340.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest, DISABLED_PrerenderPageRemovingLink) {
  set_loader_path("files/prerender/prerender_loader_removing_links.html");
  set_loader_query_and_fragment("?links_to_insert=1");
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CANCELLED, 1);

  // No ChannelDestructionWatcher is needed here, since prerenders in the
  // PrerenderLinkManager should be deleted by removing the links, rather than
  // shutting down the renderer process.
  RemoveLinkElement(0);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Flaky, http://crbug.com/167340.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest, DISABLED_PrerenderPageRemovingLinkWithTwoLinks) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  set_loader_path("files/prerender/prerender_loader_removing_links.html");
  set_loader_query_and_fragment("?links_to_insert=2");
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CANCELLED, 1);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  RemoveLinkElement(1);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

#if defined(OS_WIN)
// TODO(gavinp): Fails on XP Rel - http://crbug.com/128841
#define MAYBE_PrerenderPageRemovingLinkWithTwoLinksRemovingOne \
    DISABLED_PrerenderPageRemovingLinkWithTwoLinksRemovingOne
#else
#define MAYBE_PrerenderPageRemovingLinkWithTwoLinksRemovingOne \
    PrerenderPageRemovingLinkWithTwoLinksRemovingOne
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    MAYBE_PrerenderPageRemovingLinkWithTwoLinksRemovingOne) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;
  set_loader_path("files/prerender/prerender_loader_removing_links.html");
  set_loader_query_and_fragment("?links_to_insert=2");
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_USED, 1);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that the visibility API works.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderVisibility) {
  PrerenderTestURL("files/prerender/prerender_visibility.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the prerendering of a page is canceled correctly if we try to
// swap it in before it commits.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoCommitNoSwap) {
  // Navigate to a page that triggers a prerender for a URL that never commits.
  const GURL kNoCommitUrl("http://never-respond.example.com");
  base::FilePath file(GetTestPath("prerender_page.html"));

  base::RunLoop prerender_start_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateHangingFirstRequestProtocolHandlerOnIO,
                 kNoCommitUrl, file, prerender_start_loop.QuitClosure()));
  DisableJavascriptCalls();
  PrerenderTestURL(kNoCommitUrl,
                   FINAL_STATUS_NAVIGATION_UNCOMMITTED,
                   0);
  // Wait for the hanging request to be scheduled.
  prerender_start_loop.Run();

  // Navigate to the URL, but assume the contents won't be swapped in.
  NavigateToDestURLWithDisposition(CURRENT_TAB, false);
}

// Checks that client redirects don't add alias URLs until after they commit.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoCommitNoSwap2) {
  // Navigate to a page that then navigates to a URL that never commits.
  const GURL kNoCommitUrl("http://never-respond.example.com");
  base::FilePath file(GetTestPath("prerender_page.html"));

  base::RunLoop prerender_start_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateHangingFirstRequestProtocolHandlerOnIO,
                 kNoCommitUrl, file, prerender_start_loop.QuitClosure()));
  DisableJavascriptCalls();
  PrerenderTestURL(CreateClientRedirect(kNoCommitUrl.spec()),
                   FINAL_STATUS_APP_TERMINATING, 1);
  // Wait for the hanging request to be scheduled.
  prerender_start_loop.Run();

  // Navigating to the second URL should not swap.
  NavigateToURLWithDisposition(kNoCommitUrl, CURRENT_TAB, false);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertBeforeOnload) {
  PrerenderTestURL("files/prerender/prerender_alert_before_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT,
                   0);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertAfterOnload) {
  PrerenderTestURL("files/prerender/prerender_alert_after_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT,
                   1);
}

// Checks that plugins are not loaded while a page is being preloaded, but
// are loaded when the page is displayed.
#if defined(USE_AURA) && !defined(OS_WIN)
// http://crbug.com/103496
#define MAYBE_PrerenderDelayLoadPlugin DISABLED_PrerenderDelayLoadPlugin
#elif defined(OS_MACOSX)
// http://crbug.com/100514
#define MAYBE_PrerenderDelayLoadPlugin DISABLED_PrerenderDelayLoadPlugin
#elif defined(OS_WIN) && defined(ARCH_CPU_X86_64)
// TODO(jschuh): Failing plugin tests. crbug.com/244653
#define MAYBE_PrerenderDelayLoadPlugin DISABLED_PrerenderDelayLoadPlugin
#elif defined(OS_LINUX)
// http://crbug.com/306715
#define MAYBE_PrerenderDelayLoadPlugin DISABLED_PrerenderDelayLoadPlugin
#else
#define MAYBE_PrerenderDelayLoadPlugin PrerenderDelayLoadPlugin
#endif
// http://crbug.com/306715
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MAYBE_PrerenderDelayLoadPlugin) {
  PrerenderTestURL("files/prerender/plugin_delay_load.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that plugins are not loaded on prerendering pages when click-to-play
// is enabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickToPlay) {
  // Enable click-to-play.
  HostContentSettingsMap* content_settings_map =
      current_browser()->profile()->GetHostContentSettingsMap();
  content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ASK);

  PrerenderTestURL("files/prerender/prerender_plugin_click_to_play.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that we don't load a NaCl plugin when NaCl is disabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNaClPluginDisabled) {
  PrerenderTestURL("files/prerender/prerender_plugin_nacl_disabled.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();


  // Run this check again.  When we try to load aa ppapi plugin, the
  // "loadstart" event is asynchronously posted to a message loop.
  // It's possible that earlier call could have been run before the
  // the "loadstart" event was posted.
  // TODO(mmenke):  While this should reliably fail on regressions, the
  //                reliability depends on the specifics of ppapi plugin
  //                loading.  It would be great if we could avoid that.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));
}

// Checks that plugins in an iframe are not loaded while a page is
// being preloaded, but are loaded when the page is displayed.
#if defined(USE_AURA) && !defined(OS_WIN)
// http://crbug.com/103496
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#elif defined(OS_MACOSX)
// http://crbug.com/100514
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#elif defined(OS_WIN) && defined(ARCH_CPU_X86_64)
// TODO(jschuh): Failing plugin tests. crbug.com/244653
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#else
#define MAYBE_PrerenderIframeDelayLoadPlugin PrerenderIframeDelayLoadPlugin
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_PrerenderIframeDelayLoadPlugin) {
  PrerenderTestURL("files/prerender/prerender_iframe_plugin_delay_load.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("files/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED,
                   0);
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToFirst) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   2);
  NavigateToDestURL();
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecond) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   2);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that redirects with location.replace do not cancel a prerender and
// and swap when navigating to the first page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderLocationReplaceNavigateToFirst) {
  PrerenderTestURL("files/prerender/prerender_location_replace.html",
                   FINAL_STATUS_USED,
                   2);
  NavigateToDestURL();
}

// Checks that redirects with location.replace do not cancel a prerender and
// and swap when navigating to the second.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderLocationReplaceNavigateToSecond) {
  PrerenderTestURL("files/prerender/prerender_location_replace.html",
                   FINAL_STATUS_USED,
                   2);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that we get the right PPLT histograms for client redirect prerenders
// and navigations when the referring page is Google.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderLocationReplaceGWSHistograms) {
  DisableJavascriptCalls();
  UMAHistogramHelper histograms;

  // The loader page should look like Google.
  const std::string kGoogleDotCom("www.google.com");
  SetLoaderHostOverride(kGoogleDotCom);
  set_loader_path("files/prerender/prerender_loader_with_replace_state.html");

  GURL dest_url = GetCrossDomainTestUrl(
      "files/prerender/prerender_deferred_image.html");

  GURL prerender_url = test_server()->GetURL(
      "files/prerender/prerender_location_replace.html?" +
      net::EscapeQueryParamValue(dest_url.spec(), false) +
      "#prerender");
  GURL::Replacements replacements;
  replacements.SetHostStr(kGoogleDotCom);
  prerender_url = prerender_url.ReplaceComponents(replacements);

  // The prerender will not completely load until after the swap, so wait for a
  // title change before calling DidPrerenderPass.
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL(prerender_url, FINAL_STATUS_USED, 1);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  EXPECT_EQ(1, prerender->number_of_loads());

  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  // Although there is a client redirect, it is dropped from histograms because
  // it is a Google URL. The target page itself does not load until after the
  // swap.
  histograms.ExpectTotalCount("Prerender.gws_PrerenderNotSwappedInPLT", 0);

  GURL navigate_url = test_server()->GetURL(
      "files/prerender/prerender_location_replace.html?" +
      net::EscapeQueryParamValue(dest_url.spec(), false) +
      "#navigate");
  navigate_url = navigate_url.ReplaceComponents(replacements);

  NavigationOrSwapObserver swap_observer(
      current_browser()->tab_strip_model(),
      GetActiveWebContents(), 2);
  current_browser()->OpenURL(OpenURLParams(
      navigate_url, Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
  swap_observer.Wait();

  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));

  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.gws_PrerenderNotSwappedInPLT", 0);
  histograms.ExpectTotalCount("Prerender.gws_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.gws_PerceivedPLTMatched", 1);
  histograms.ExpectTotalCount(
      "Prerender.gws_PerceivedPLTMatchedComplete", 1);

  // The client redirect does /not/ count as a miss because it's a Google URL.
  histograms.ExpectTotalCount("Prerender.PerceivedPLTFirstAfterMiss", 0);
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection via a mouse click.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecondViaClick) {
  GURL prerender_url = test_server()->GetURL(
      CreateClientRedirect("files/prerender/prerender_page.html"));
  GURL destination_url = test_server()->GetURL(
      "files/prerender/prerender_page.html");
  PrerenderTestURL(prerender_url, FINAL_STATUS_USED, 2);
  OpenURLViaClick(destination_url);
}

// Checks that a page served over HTTPS is correctly prerendered.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttps) {
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that client-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectInIframe) {
  std::string redirect_path = CreateClientRedirect(
      "/files/prerender/prerender_embedded_content.html");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 2);
  EXPECT_FALSE(UrlIsInPrerenderManager(
      "files/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToFirst) {
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecond) {
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection via a mouse click.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecondViaClick) {
  GURL prerender_url = test_server()->GetURL(
      CreateServerRedirect("files/prerender/prerender_page.html"));
  GURL destination_url = test_server()->GetURL(
      "files/prerender/prerender_page.html");
  PrerenderTestURL(prerender_url, FINAL_STATUS_USED, 1);
  OpenURLViaClick(destination_url);
}

// Checks that server-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderServerRedirectInIframe) {
  std::string redirect_path = CreateServerRedirect(
      "/files/prerender/prerender_embedded_content.html");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(
      "files/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Prerenders a page that contains an automatic download triggered through an
// iframe. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadIframe) {
  PrerenderTestURL("files/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_DOWNLOAD,
                   0);
}

// Prerenders a page that contains an automatic download triggered through
// Javascript changing the window.location. This should not prerender
// successfully
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadLocation) {
  PrerenderTestURL(CreateClientRedirect("files/download-test1.lib"),
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through a
// client-issued redirect. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadClientRedirect) {
  PrerenderTestURL("files/prerender/prerender_download_refresh.html",
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("files/prerender/prerender_referrer.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderNoSSLReferrer) {
  UseHttpsSrcServer();
  PrerenderTestURL("files/prerender/prerender_no_referrer.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the referrer is set when prerendering is cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelReferrer) {
  scoped_ptr<TestContentBrowserClient> test_content_browser_client(
      new TestContentBrowserClient);
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(test_content_browser_client.get());

  PrerenderTestURL("files/prerender/prerender_referrer.html",
                   FINAL_STATUS_CANCELLED,
                   1);
  OpenDestURLViaClick();

  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));

  content::SetBrowserClientForTesting(original_browser_client);
}

// Checks that popups on a prerendered page cause cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPopup) {
  PrerenderTestURL("files/prerender/prerender_popup.html",
                   FINAL_STATUS_CREATE_NEW_WINDOW,
                   0);
}

// Checks that registering a protocol handler causes cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRegisterProtocolHandler) {
  PrerenderTestURL("files/prerender/prerender_register_protocol_handler.html",
                   FINAL_STATUS_REGISTER_PROTOCOL_HANDLER,
                   0);
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExcessiveMemory) {
  ASSERT_TRUE(GetPrerenderManager());
  GetPrerenderManager()->mutable_config().max_bytes = 30 * 1024 * 1024;
  // The excessive memory kill may happen before or after the load event as it
  // happens asynchronously with IPC calls. Even if the test does not start
  // allocating until after load, the browser process might notice before the
  // message gets through. This happens on XP debug bots because they're so
  // slow. Instead, don't bother checking the load event count.
  DisableLoadEventCheck();
  PrerenderTestURL("files/prerender/prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED, 0);
}

// Checks shutdown code while a prerender is active.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderQuickQuit) {
  DisableJavascriptCalls();
  DisableLoadEventCheck();
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING,
                   0);
}

// Checks that we don't prerender in an infinite loop.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderInfiniteLoop) {
  const char* const kHtmlFileA = "files/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "files/prerender/prerender_infinite_b.html";

  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  ScopedVector<TestPrerender> prerenders =
      PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);
  ASSERT_TRUE(prerenders[0]->contents());
  // Assert that the pending prerender is in there already. This relies on the
  // fact that the renderer sends out the AddLinkRelPrerender IPC before sending
  // the page load one.
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next url is now in the manager and
  // not pending. This relies on pending prerenders being resolved in the same
  // event loop iteration as OnPrerenderStop.
  EXPECT_TRUE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_EQ(1U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());
}

// Checks that we don't prerender in an infinite loop and multiple links are
// handled correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderInfiniteLoopMultiple) {
  const char* const kHtmlFileA =
      "files/prerender/prerender_infinite_a_multiple.html";
  const char* const kHtmlFileB =
      "files/prerender/prerender_infinite_b_multiple.html";
  const char* const kHtmlFileC =
      "files/prerender/prerender_infinite_c_multiple.html";

  // This test is conceptually simplest if concurrency is at two, since we
  // don't have to worry about which of kHtmlFileB or kHtmlFileC gets evicted.
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  ScopedVector<TestPrerender> prerenders =
      PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);
  ASSERT_TRUE(prerenders[0]->contents());

  // Next url should be in pending list but not an active entry. This relies on
  // the fact that the renderer sends out the AddLinkRelPrerender IPC before
  // sending the page load one.
  EXPECT_EQ(3U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileC));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next urls are now in the manager
  // and not pending. One and only one of the URLs (the last seen) should be the
  // active entry. This relies on pending prerenders being resolved in the same
  // event loop iteration as OnPrerenderStop.
  bool url_b_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileB);
  bool url_c_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileC);
  EXPECT_TRUE(url_b_is_active_prerender && url_c_is_active_prerender);
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(2U, GetRunningLinkPrerenderCount());
}

// Checks that pending prerenders are aborted (and never launched) when launched
// by a prerender that itself gets aborted.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAbortPendingOnCancel) {
  const char* const kHtmlFileA = "files/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "files/prerender/prerender_infinite_b.html";

  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL(kHtmlFileA, FINAL_STATUS_CANCELLED, 1);
  ASSERT_TRUE(prerender->contents());
  // Assert that the pending prerender is in there already. This relies on the
  // fact that the renderer sends out the AddLinkRelPrerender IPC before sending
  // the page load one.
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));

  // Cancel the prerender.
  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  // All prerenders are now gone.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderTaskManager) {
  // Show the task manager. This populates the model.
  chrome::OpenTaskManager(current_browser());
  // Wait for the model of task manager to start.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);

  // Start with two resources.
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // One of the resources that has a WebContents associated with it should have
  // the Prerender prefix.
  const base::string16 prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX,
                                 base::string16());
  base::string16 prerender_title;
  int num_prerender_tabs = 0;

  TaskManagerModel* model = GetModel();
  // The task manager caches values. Force the titles to be fresh.
  model->Refresh();
  for (int i = 0; i < model->ResourceCount(); ++i) {
    if (model->GetResourceWebContents(i)) {
      prerender_title = model->GetResourceTitle(i);
      if (StartsWith(prerender_title, prefix, true))
        ++num_prerender_tabs;
    }
  }
  EXPECT_EQ(1, num_prerender_tabs);
  const base::string16 prerender_page_title =
      prerender_title.substr(prefix.length());

  NavigateToDestURL();

  // There should be no tabs with the Prerender prefix.
  const base::string16 tab_prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX, base::string16());
  num_prerender_tabs = 0;
  int num_tabs_with_prerender_page_title = 0;
  model->Refresh();
  for (int i = 0; i < model->ResourceCount(); ++i) {
    if (model->GetResourceWebContents(i)) {
      base::string16 tab_title = model->GetResourceTitle(i);
      if (StartsWith(tab_title, prefix, true)) {
        ++num_prerender_tabs;
      } else {
        EXPECT_TRUE(StartsWith(tab_title, tab_prefix, true));

        // The prerender tab should now be a normal tab but the title should be
        // the same. Depending on timing, there may be more than one of these.
        const base::string16 tab_page_title =
            tab_title.substr(tab_prefix.length());
        if (prerender_page_title.compare(tab_page_title) == 0)
          ++num_tabs_with_prerender_page_title;
      }
    }
  }
  EXPECT_EQ(0, num_prerender_tabs);

  // We may have deleted the prerender tab, but the swapped in tab should be
  // active.
  EXPECT_GE(num_tabs_with_prerender_page_title, 1);
  EXPECT_LE(num_tabs_with_prerender_page_title, 2);
}

// Checks that audio loads are deferred on prerendering.
// Times out under AddressSanitizer, see http://crbug.com/108402
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderHTML5Audio) {
  PrerenderTestURL("files/prerender/prerender_html5_audio.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that audio loads are deferred on prerendering and played back when
// the prerender is swapped in if autoplay is set.
// Periodically fails on chrome-os.  See http://crbug.com/145263
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderHTML5AudioAutoplay) {
  PrerenderTestURL("files/prerender/prerender_html5_audio_autoplay.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that audio loads are deferred on prerendering and played back when
// the prerender is swapped in if js starts playing.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderHTML5AudioJsplay) {
  PrerenderTestURL("files/prerender/prerender_html5_audio_jsplay.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that video loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderHTML5Video) {
  PrerenderTestURL("files/prerender/prerender_html5_video.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that video tags inserted by javascript are deferred and played
// correctly on swap in.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderHTML5VideoJs) {
  PrerenderTestURL("files/prerender/prerender_html5_video_script.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks for correct network events by using a busy sleep the javascript.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderHTML5VideoNetwork) {
  DisableJavascriptCalls();
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_html5_video_network.html",
                       FINAL_STATUS_USED,
                       1);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that scripts can retrieve the correct window size while prerendering.
#if defined(TOOLKIT_VIEWS)
// TODO(beng): Widget hierarchy split causes this to fail http://crbug.com/82363
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderWindowSize) {
#else
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderWindowSize) {
#endif
  PrerenderTestURL("files/prerender/prerender_size.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that prerenderers will terminate when the RenderView crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRendererCrash) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page.html",
                       FINAL_STATUS_RENDERER_CRASHED,
                       1);

  // Navigate to about:crash and then wait for the renderer to crash.
  ASSERT_TRUE(prerender->contents());
  ASSERT_TRUE(prerender->contents()->prerender_contents());
  prerender->contents()->prerender_contents()->GetController().
      LoadURL(
          GURL(content::kChromeUICrashURL),
          content::Referrer(),
          content::PAGE_TRANSITION_TYPED,
          std::string());
  prerender->WaitForStop();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageWithFragment) {
  PrerenderTestURL("files/prerender/prerender_page.html#fragment",
                   FINAL_STATUS_USED,
                   1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()->tab_strip_model()->
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageWithRedirectedFragment) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/prerender_page.html#fragment"),
      FINAL_STATUS_USED,
      2);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()->tab_strip_model()->
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that we do not use a prerendered page when navigating from
// the main page to a fragment.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageNavigateFragment) {
  PrerenderTestURL("files/prerender/no_prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING,
                   1);
  NavigateToURLWithDisposition(
      "files/prerender/no_prerender_page.html#fragment",
      CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when we prerender a fragment
// but navigate to the main page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderFragmentNavigatePage) {
  PrerenderTestURL("files/prerender/no_prerender_page.html#fragment",
                   FINAL_STATUS_APP_TERMINATING,
                   1);
  NavigateToURLWithDisposition(
      "files/prerender/no_prerender_page.html",
      CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when we prerender a fragment
// but navigate to a different fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderFragmentNavigateFragment) {
  PrerenderTestURL("files/prerender/no_prerender_page.html#other_fragment",
                   FINAL_STATUS_APP_TERMINATING,
                   1);
  NavigateToURLWithDisposition(
      "files/prerender/no_prerender_page.html#fragment",
      CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when the page uses a client
// redirect to refresh from a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectFromFragment) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/no_prerender_page.html#fragment"),
      FINAL_STATUS_APP_TERMINATING,
      2);
  NavigateToURLWithDisposition(
      "files/prerender/no_prerender_page.html",
      CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when the page uses a client
// redirect to refresh to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectToFragment) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/no_prerender_page.html"),
      FINAL_STATUS_APP_TERMINATING,
      2);
  NavigateToURLWithDisposition(
      "files/prerender/no_prerender_page.html#fragment",
      CURRENT_TAB, false);
}

// Checks that we correctly use a prerendered page when the page uses JS to set
// the window.location.hash to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageChangeFragmentLocationHash) {
  PrerenderTestURL("files/prerender/prerender_fragment_location_hash.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_fragment_location_hash.html");
}

// Checks that prerendering a PNG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImagePng) {
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/image.png", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that prerendering a JPG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImageJpeg) {
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/image.jpeg", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that a prerender of a CRX will result in a cancellation due to
// download.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCrx) {
  PrerenderTestURL("files/prerender/extension.crx", FINAL_STATUS_DOWNLOAD, 0);
}

// Checks that xhr GET requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrGet) {
  PrerenderTestURL("files/prerender/prerender_xhr_get.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr HEAD requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrHead) {
  PrerenderTestURL("files/prerender/prerender_xhr_head.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr OPTIONS requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrOptions) {
  PrerenderTestURL("files/prerender/prerender_xhr_options.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr TRACE requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrTrace) {
  PrerenderTestURL("files/prerender/prerender_xhr_trace.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr POST requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPost) {
  PrerenderTestURL("files/prerender/prerender_xhr_post.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr PUT cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPut) {
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that xhr DELETE cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrDelete) {
  PrerenderTestURL("files/prerender/prerender_xhr_delete.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that a top-level page which would trigger an SSL error is canceled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorTopLevel) {
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.server_certificate =
      net::SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_SSL_ERROR,
                   0);
}

// Checks that an SSL error that comes from a subresource does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorSubresource) {
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.server_certificate =
      net::SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/image.jpeg");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that an SSL error that comes from an iframe does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorIframe) {
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.server_certificate =
      net::SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(
      "files/prerender/prerender_embedded_content.html");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that we cancel correctly when window.print() is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPrint) {
  PrerenderTestURL("files/prerender/prerender_print.html",
                   FINAL_STATUS_WINDOW_PRINT,
                   0);
}

// Checks that if a page is opened in a new window by javascript and both the
// pages are in the same domain, the prerendered page is not used, due to
// window.opener.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSameDomainWindowOpenerWindowOpen) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_WINDOW_OPENER,
                   1);
  OpenDestURLViaWindowOpen();
}

// Checks that if a page is opened due to click on a href with target="_blank"
// and both pages are in the same domain the prerendered page is not used, due
// to window.opener.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSameDomainWindowOpenerClickTarget) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_WINDOW_OPENER,
                   1);
  OpenDestURLViaClickTarget();
}

class TestClientCertStore : public net::ClientCertStore {
 public:
  TestClientCertStore() {}
  virtual ~TestClientCertStore() {}

  // net::ClientCertStore:
  virtual void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                              net::CertificateList* selected_certs,
                              const base::Closure& callback) OVERRIDE {
    *selected_certs = net::CertificateList(
        1, scoped_refptr<net::X509Certificate>(
        new net::X509Certificate("test", "test", base::Time(), base::Time())));
    callback.Run();
  }
};

scoped_ptr<net::ClientCertStore> CreateCertStore() {
  return scoped_ptr<net::ClientCertStore>(new TestClientCertStore);
}

// Checks that a top-level page which would normally request an SSL client
// certificate will never be seen since it's an https top-level resource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLClientCertTopLevel) {
  ProfileIOData::FromResourceContext(
      current_browser()->profile()->GetResourceContext())->
          set_client_cert_store_factory_for_testing(
              base::Bind(&CreateCertStore));
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url, FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED, 0);
}

// Checks that an SSL Client Certificate request that originates from a
// subresource will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLClientCertSubresource) {
  ProfileIOData::FromResourceContext(
      current_browser()->profile()->GetResourceContext())->
          set_client_cert_store_factory_for_testing(
              base::Bind(&CreateCertStore));
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/image.jpeg");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED,
                   0);
}

// Checks that an SSL Client Certificate request that originates from an
// iframe will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLClientCertIframe) {
  ProfileIOData::FromResourceContext(
      current_browser()->profile()->GetResourceContext())->
          set_client_cert_store_factory_for_testing(
              base::Bind(&CreateCertStore));
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(
      "files/prerender/prerender_embedded_content.html");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED,
                   0);
}

#if defined(FULL_SAFE_BROWSING)
// Ensures that we do not prerender pages with a safe browsing
// interstitial.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingTopLevel) {
  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, SB_THREAT_TYPE_URL_MALWARE);
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_SAFE_BROWSING, 0);
}

// Ensures that server redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSafeBrowsingServerRedirect) {
  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, SB_THREAT_TYPE_URL_MALWARE);
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Ensures that client redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSafeBrowsingClientRedirect) {
  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, SB_THREAT_TYPE_URL_MALWARE);
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING,
                   1);
}

// Ensures that we do not prerender pages which have a malware subresource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingSubresource) {
  GURL image_url = test_server()->GetURL("files/prerender/image.jpeg");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      image_url, SB_THREAT_TYPE_URL_MALWARE);
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Ensures that we do not prerender pages which have a malware iframe.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingIframe) {
  GURL iframe_url = test_server()->GetURL(
      "files/prerender/prerender_embedded_content.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      iframe_url, SB_THREAT_TYPE_URL_MALWARE);
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", iframe_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SAFE_BROWSING,
                   0);
}

#endif

// Checks that a local storage read will not cause prerender to fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderLocalStorageRead) {
  PrerenderTestURL("files/prerender/prerender_localstorage_read.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that a local storage write will not cause prerender to fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderLocalStorageWrite) {
  PrerenderTestURL("files/prerender/prerender_localstorage_write.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the favicon is properly loaded on prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFavicon) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_favicon.html",
                       FINAL_STATUS_USED,
                       1);
  NavigateToDestURL();

  if (!FaviconTabHelper::FromWebContents(
          GetActiveWebContents())->FaviconIsValid()) {
    // If the favicon has not been set yet, wait for it to be.
    content::WindowedNotificationObserver favicon_update_watcher(
        chrome::NOTIFICATION_FAVICON_UPDATED,
        content::Source<WebContents>(GetActiveWebContents()));
    favicon_update_watcher.Wait();
  }
  EXPECT_TRUE(FaviconTabHelper::FromWebContents(
      GetActiveWebContents())->FaviconIsValid());
}

// Checks that when a prerendered page is swapped in to a referring page, the
// unload handlers on the referring page are executed.
// Fails about 50% on CrOS, 5-10% on linux, win, mac. http://crbug.com/128986
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderUnload) {
  set_loader_path("files/prerender/prerender_loader_with_unload.html");
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  base::string16 expected_title = base::ASCIIToUTF16("Unloaded");
  content::TitleWatcher title_watcher(
      current_browser()->tab_strip_model()->GetActiveWebContents(),
      expected_title);
  NavigateToDestURL();
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Checks that when the history is cleared, prerendering is cancelled and
// prerendering history is cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearHistory) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page.html",
                       FINAL_STATUS_CACHE_OR_HISTORY_CLEARED,
                       1);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClearBrowsingData, current_browser(),
                 BrowsingDataRemover::REMOVE_HISTORY));
  prerender->WaitForStop();

  // Make sure prerender history was cleared.
  EXPECT_EQ(0, GetHistoryLength());
}

// Disabled due to flakiness: crbug.com/316225
// Checks that when the cache is cleared, prerenders are cancelled but
// prerendering history is not cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderClearCache) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page.html",
                       FINAL_STATUS_CACHE_OR_HISTORY_CLEARED,
                       1);

  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ClearBrowsingData, current_browser(),
                 BrowsingDataRemover::REMOVE_CACHE));
  prerender->WaitForStop();

  // Make sure prerender history was not cleared.  Not a vital behavior, but
  // used to compare with PrerenderClearHistory test.
  EXPECT_EQ(1, GetHistoryLength());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelAll) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page.html",
                       FINAL_STATUS_CANCELLED,
                       1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderEvents) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page.html",
                       FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_TRUE(DidReceivePrerenderStartEventForLinkNumber(0));
  EXPECT_TRUE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_FALSE(HadPrerenderEventErrors());
}

// Cancels the prerender of a page with its own prerender.  The second prerender
// should never be started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelPrerenderWithPrerender) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_infinite_a.html",
                       FINAL_STATUS_CANCELLED,
                       1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

// Prerendering and history tests.
// The prerendered page is navigated to in several ways [navigate via
// omnibox, click on link, key-modified click to open in background tab, etc],
// followed by a navigation to another page from the prerendered page, followed
// by a back navigation.

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNavigateClickGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  ClickToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNavigateNavigateGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  NavigateToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickClickGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  OpenDestURLViaClick();
  ClickToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNavigateGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  OpenDestURLViaClick();
  NavigateToNextPageAfterPrerender();
  GoBackToPrerender();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewWindow) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_WINDOW_OPENER,
                   1);
  OpenDestURLViaClickNewWindow();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewForegroundTab) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_WINDOW_OPENER,
                   1);
  OpenDestURLViaClickNewForegroundTab();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewBackgroundTab) {
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                       FINAL_STATUS_WINDOW_OPENER,
                       1);
  ASSERT_TRUE(prerender->contents());
  prerender->contents()->set_should_be_shown(false);
  OpenDestURLViaClickNewBackgroundTab();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NavigateToPrerenderedPageWhenDevToolsAttached) {
  DisableJavascriptCalls();
  WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetOrCreateFor(
      web_contents->GetRenderViewHost()));
  DevToolsManager* manager = DevToolsManager::GetInstance();
  FakeDevToolsClientHost client_host;
  manager->RegisterDevToolsClientHostFor(agent.get(), &client_host);
  const char* url = "files/prerender/prerender_page.html";
  PrerenderTestURL(url, FINAL_STATUS_DEVTOOLS_ATTACHED, 1);
  NavigateToURLWithDisposition(url, CURRENT_TAB, false);
  manager->ClientHostClosing(&client_host);
}

// Validate that the sessionStorage namespace remains the same when swapping
// in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSessionStorage) {
  set_loader_path("files/prerender/prerender_loader_with_session_storage.html");
  PrerenderTestURL(GetCrossDomainTestUrl("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  GoBackToPageBeforePrerender();
}

// Checks that the control group works.  An XHR PUT cannot be detected in the
// control group.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ControlGroup) {
  RestorePrerenderMode restore_prerender_mode;
  PrerenderManager::SetMode(
      PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   FINAL_STATUS_WOULD_HAVE_BEEN_USED, 0);
  NavigateToDestURL();
}

// Checks that the control group correctly hits WOULD_HAVE_BEEN_USED
// renderer-initiated navigations. (This verifies that the ShouldFork logic
// behaves correctly.)
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ControlGroupRendererInitiated) {
  RestorePrerenderMode restore_prerender_mode;
  PrerenderManager::SetMode(
      PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   FINAL_STATUS_WOULD_HAVE_BEEN_USED, 0);
  OpenDestURLViaClick();
}

// Make sure that the MatchComplete dummy works in the normal case.  Once
// a prerender is cancelled because of a script, a dummy must be created to
// account for the MatchComplete case, and it must have a final status of
// FINAL_STATUS_WOULD_HAVE_BEEN_USED.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MatchCompleteDummy) {
  UMAHistogramHelper histograms;

  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_INVALID_HTTP_METHOD);
  expected_final_status_queue.push_back(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   expected_final_status_queue, 1);
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  histograms.ExpectTotalCount("Prerender.websame_PrerenderNotSwappedInPLT", 1);

  NavigateToDestURL();
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount(
      "Prerender.websame_PerceivedPLTMatchedComplete", 1);
}

// Verify that a navigation that hits a MatchComplete dummy while another is in
// progress does not also classify the previous navigation as a MatchComplete.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MatchCompleteDummyCancelNavigation) {
  UMAHistogramHelper histograms;

  // Arrange for a URL to hang.
  const GURL kNoCommitUrl("http://never-respond.example.com");
  base::FilePath file(FILE_PATH_LITERAL(
      "chrome/test/data/prerender/prerender_page.html"));
  base::RunLoop hang_loop;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateHangingFirstRequestProtocolHandlerOnIO,
                 kNoCommitUrl, file, hang_loop.QuitClosure()));

  // First, fire a prerender that aborts after it completes its load.
  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_INVALID_HTTP_METHOD);
  expected_final_status_queue.push_back(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   expected_final_status_queue, 1);
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  histograms.ExpectTotalCount("Prerender.websame_PrerenderNotSwappedInPLT", 1);

  // Open the hanging URL in a new tab. Wait for both the new tab to open and
  // the hanging request to be scheduled.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), kNoCommitUrl, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  hang_loop.Run();

  // Now interrupt that navigation and navigate to the destination URL. This
  // should forcibly complete the previous navigation and also complete a
  // WOULD_HAVE_BEEN_PRERENDERED navigation.
  NavigateToDestURL();
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 2);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount(
      "Prerender.websame_PerceivedPLTMatchedComplete", 1);
}

class PrerenderBrowserTestWithNaCl : public PrerenderBrowserTest {
 public:
  PrerenderBrowserTestWithNaCl() {}
  virtual ~PrerenderBrowserTestWithNaCl() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PrerenderBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableNaCl);
  }
};

// Check that NaCl plugins work when enabled, with prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithNaCl,
                       PrerenderNaClPluginEnabled) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  PrerenderTestURL("files/prerender/prerender_plugin_nacl_enabled.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();

  // To avoid any chance of a race, we have to let the script send its response
  // asynchronously.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool display_test_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents,
                                                   "DidDisplayReallyPass()",
                                                   &display_test_result));
  ASSERT_TRUE(display_test_result);
}

// Checks that the referrer policy is used when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrerPolicy) {
  set_loader_path("files/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("files/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the referrer policy is used when prerendering on HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLReferrerPolicy) {
  UseHttpsSrcServer();
  set_loader_path("files/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("files/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the referrer policy is used when prerendering is cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelReferrerPolicy) {
  scoped_ptr<TestContentBrowserClient> test_content_browser_client(
      new TestContentBrowserClient);
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(test_content_browser_client.get());

  set_loader_path("files/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("files/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_CANCELLED,
                   1);
  OpenDestURLViaClick();

  bool display_test_result = false;
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(DidDisplayPass())",
      &display_test_result));
  EXPECT_TRUE(display_test_result);

  content::SetBrowserClientForTesting(original_browser_client);
}

// Test interaction of the webNavigation and tabs API with prerender.
class PrerenderBrowserTestWithExtensions : public PrerenderBrowserTest,
                                           public ExtensionApiTest {
 public:
  PrerenderBrowserTestWithExtensions() {
    // The individual tests start the test server through ExtensionApiTest, so
    // the port number can be passed through to the extension.
    autostart_test_server_ = false;
  }

  virtual void SetUp() OVERRIDE {
    PrerenderBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PrerenderBrowserTest::SetUpCommandLine(command_line);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    PrerenderBrowserTest::SetUpInProcessBrowserTestFixture();
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    PrerenderBrowserTest::TearDownInProcessBrowserTestFixture();
    ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    PrerenderBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithExtensions, WebNavigation) {
  ASSERT_TRUE(StartSpawnedTestServer());
  extensions::FrameNavigationState::set_allow_extension_scheme(true);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kAllowLegacyExtensionManifests);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/prerender")) << message_;

  ResultCatcher catcher;

  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()->tab_strip_model()->
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Fails often on Windows dbg bots. http://crbug.com/177163
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TabsApi DISABLED_TabsApi
#else
#define MAYBE_TabsApi TabsApi
#endif  // defined(OS_WIN) && !defined(NDEBUG)
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithExtensions, MAYBE_TabsApi) {
  ASSERT_TRUE(StartSpawnedTestServer());
  extensions::FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionSubtest("tabs/on_replaced", "on_replaced.html"))
      << message_;

  ResultCatcher catcher;

  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()->tab_strip_model()->
      GetActiveWebContents()->GetRenderProcessHost());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Checks that non-http/https/chrome-extension subresource cancels the
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelSubresourceUnsupportedScheme) {
  GURL image_url = GURL("invalidscheme://www.google.com/test.jpg");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Ensure that about:blank is permitted for any subresource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderAllowAboutBlankSubresource) {
  GURL image_url = GURL("about:blank");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that non-http/https/chrome-extension subresource cancels the prerender
// on redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelSubresourceRedirectUnsupportedScheme) {
  GURL image_url = test_server()->GetURL(
      CreateServerRedirect("invalidscheme://www.google.com/test.jpg"));
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Checks that chrome-extension subresource does not cancel the prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderKeepSubresourceExtensionScheme) {
  GURL image_url = GURL("chrome-extension://abcdefg/test.jpg");
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that redirect to chrome-extension subresource does not cancel the
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderKeepSubresourceRedirectExtensionScheme) {
  GURL image_url = test_server()->GetURL(
      CreateServerRedirect("chrome-extension://abcdefg/test.jpg"));
  std::vector<net::SpawnedTestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::SpawnedTestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}


// Checks that non-http/https main page redirects cancel the prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelMainFrameRedirectUnsupportedScheme) {
  GURL url = test_server()->GetURL(
      CreateServerRedirect("invalidscheme://www.google.com/test.html"));
  PrerenderTestURL(url, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Checks that media source video loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5MediaSourceVideo) {
  PrerenderTestURL("files/prerender/prerender_html5_video_media_source.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  WaitForASCIITitle(GetActiveWebContents(), kPassTitle);
}

// Checks that a prerender that creates an audio stream (via a WebAudioDevice)
// is cancelled.
// http://crbug.com/261489
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderWebAudioDevice) {
  PrerenderTestURL("files/prerender/prerender_web_audio_device.html",
                   FINAL_STATUS_CREATING_AUDIO_STREAM, 1);
}

// Checks that prerenders do not swap in to WebContents being captured.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCapturedWebContents) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_PAGE_BEING_CAPTURED, 1);
  WebContents* web_contents = GetActiveWebContents();
  web_contents->IncrementCapturerCount(gfx::Size());
  NavigateToDestURLWithDisposition(CURRENT_TAB, false);
  web_contents->DecrementCapturerCount();
}

// Checks that prerenders are aborted on cross-process navigation from
// a server redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCrossProcessServerRedirect) {
  // Force everything to be a process swap.
  SwapProcessesContentBrowserClient test_browser_client;
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(&test_browser_client);

  PrerenderTestURL(
      CreateServerRedirect("files/prerender/prerender_page.html"),
      FINAL_STATUS_OPEN_URL, 0);

  content::SetBrowserClientForTesting(original_browser_client);
}

// Checks that URLRequests for prerenders being aborted on cross-process
// navigation from a server redirect are cleaned up, so they don't keep cache
// entries locked.
// See http://crbug.com/341134
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCrossProcessServerRedirectNoHang) {
  const char kDestPath[] = "files/prerender/prerender_page.html";
  // Force everything to be a process swap.
  SwapProcessesContentBrowserClient test_browser_client;
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(&test_browser_client);

  PrerenderTestURL(CreateServerRedirect(kDestPath), FINAL_STATUS_OPEN_URL, 0);

  ui_test_utils::NavigateToURL(
      browser(),
      test_server()->GetURL(kDestPath));

  content::SetBrowserClientForTesting(original_browser_client);
}

// Checks that prerenders are aborted on cross-process navigation from
// a client redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCrossProcessClientRedirect) {
  // Cross-process navigation logic for renderer-initiated navigations
  // is partially controlled by the renderer, namely
  // ChromeContentRendererClient. This test instead relies on the Web
  // Store triggering such navigations.
  std::string webstore_url = extension_urls::GetWebstoreLaunchURL();

  // Mock out requests to the Web Store.
  base::FilePath file(GetTestPath("prerender_page.html"));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateMockProtocolHandlerOnIO,
                 GURL(webstore_url), file));

  PrerenderTestURL(CreateClientRedirect(webstore_url),
                   FINAL_STATUS_OPEN_URL, 1);
}

// Checks that canceling a MatchComplete dummy doesn't result in two
// stop events.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, CancelMatchCompleteDummy) {
  std::vector<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_JAVASCRIPT_ALERT);
  expected_final_status_queue.push_back(FINAL_STATUS_CANCELLED);
  ScopedVector<TestPrerender> prerenders =
      PrerenderTestURL("files/prerender/prerender_alert_before_onload.html",
                       expected_final_status_queue, 0);

  // Cancel the MatchComplete dummy.
  GetPrerenderManager()->CancelAllPrerenders();
  prerenders[1]->WaitForStop();

  // Check the referring page only got one copy of the event.
  EXPECT_FALSE(HadPrerenderEventErrors());
}

// Checks that a deferred redirect to an image is not loaded until the page is
// visible. Also test the right histogram events are emitted in this case.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDeferredImage) {
  DisableJavascriptCalls();
  UMAHistogramHelper histograms;

  // The prerender will not completely load until after the swap, so wait for a
  // title change before calling DidPrerenderPass.
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL(
          "files/prerender/prerender_deferred_image.html",
          FINAL_STATUS_USED, 0);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_EQ(1, GetPrerenderDomContentLoadedEventCountForLinkNumber(0));
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  EXPECT_EQ(0, prerender->number_of_loads());
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
  histograms.ExpectTotalCount("Prerender.websame_PrerenderNotSwappedInPLT", 0);

  // Swap.
  NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                         GetActiveWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), dest_url(), CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  swap_observer.Wait();

  // The prerender never observes the final load.
  EXPECT_EQ(0, prerender->number_of_loads());

  // Now check DidDisplayPass.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));

  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.websame_PrerenderNotSwappedInPLT", 0);
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.websame_PerceivedPLTMatched", 1);
  histograms.ExpectTotalCount(
      "Prerender.websame_PerceivedPLTMatchedComplete", 1);
}

// Checks that a deferred redirect to an image is not loaded until the
// page is visible, even after another redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderDeferredImageAfterRedirect) {
  DisableJavascriptCalls();

  // The prerender will not completely load until after the swap, so wait for a
  // title change before calling DidPrerenderPass.
  scoped_ptr<TestPrerender> prerender =
      PrerenderTestURL(
          "files/prerender/prerender_deferred_image.html",
          FINAL_STATUS_USED, 0);
  WaitForASCIITitle(prerender->contents()->prerender_contents(), kReadyTitle);
  EXPECT_TRUE(DidPrerenderPass(prerender->contents()->prerender_contents()));
  EXPECT_EQ(0, prerender->number_of_loads());

  // Swap.
  NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                         GetActiveWebContents());
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), dest_url(), CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  swap_observer.Wait();

  // The prerender never observes the final load.
  EXPECT_EQ(0, prerender->number_of_loads());

  // Now check DidDisplayPass.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));
}

// Checks that deferred redirects in the main frame are followed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDeferredMainFrame) {
  DisableJavascriptCalls();
  PrerenderTestURL(
      "files/prerender/image-deferred.png",
      FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that deferred redirects in the main frame are followed, even
// with a double-redirect.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderDeferredMainFrameAfterRedirect) {
  DisableJavascriptCalls();
  PrerenderTestURL(
      CreateServerRedirect("files/prerender/image-deferred.png"),
      FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that deferred redirects in a synchronous XHR abort the
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDeferredSynchronousXHR) {
  PrerenderTestURL("files/prerender/prerender_deferred_sync_xhr.html",
                   FINAL_STATUS_BAD_DEFERRED_REDIRECT, 0);
  NavigateToDestURL();
}

// Checks that prerenders are not swapped for navigations with extra headers.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExtraHeadersNoSwap) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  content::OpenURLParams params(dest_url(), Referrer(), CURRENT_TAB,
                                content::PAGE_TRANSITION_TYPED, false);
  params.extra_headers = "X-Custom-Header: 42\r\n";
  NavigateToURLWithParams(params, false);
}

// Checks that prerenders are not swapped for navigations with browser-initiated
// POST data.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderBrowserInitiatedPostNoSwap) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  std::string post_data = "DATA";
  content::OpenURLParams params(dest_url(), Referrer(), CURRENT_TAB,
                                content::PAGE_TRANSITION_TYPED, false);
  params.uses_post = true;
  params.browser_initiated_post_data =
      base::RefCountedString::TakeString(&post_data);
  NavigateToURLWithParams(params, false);
}

// Checks that the prerendering of a page is canceled correctly when the
// prerendered page tries to make a second navigation entry.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNewNavigationEntry) {
  PrerenderTestURL("files/prerender/prerender_new_entry.html",
                   FINAL_STATUS_NEW_NAVIGATION_ENTRY,
                   1);
}

// Attempt a swap-in in a new tab, verifying that session storage namespace
// merging works.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNewTab) {
  // Mock out some URLs and count the number of requests to one of them. Both
  // prerender_session_storage.html and init_session_storage.html need to be
  // mocked so they are same-origin.
  const GURL kInitURL("http://prerender.test/init_session_storage.html");
  base::FilePath init_file = GetTestPath("init_session_storage.html");
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateMockProtocolHandlerOnIO, kInitURL, init_file));

  const GURL kTestURL("http://prerender.test/prerender_session_storage.html");
  base::FilePath test_file = GetTestPath("prerender_session_storage.html");
  RequestCounter counter;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateCountingProtocolHandlerOnIO,
                 kTestURL, test_file, counter.AsWeakPtr()));

  PrerenderTestURL(kTestURL, FINAL_STATUS_USED, 1);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), kInitURL, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Now navigate in the new tab. Set expect_swap_to_succeed to false because
  // the swap does not occur synchronously.
  //
  // TODO(davidben): When all swaps become asynchronous, remove the OpenURL
  // return value assertion and let this go through the usual successful-swap
  // codepath.
  NavigateToDestURLWithDisposition(CURRENT_TAB, false);

  // Verify DidDisplayPass manually since the previous call skipped it.
  EXPECT_TRUE(DidDisplayPass(
      current_browser()->tab_strip_model()->GetActiveWebContents()));

  // Only one request to the test URL started.
  //
  // TODO(davidben): Re-enable this check when the races in attaching the
  // throttle are resolved. http://crbug.com/335835
  // EXPECT_EQ(1, counter.count());
}

// Attempt a swap-in in a new tab, verifying that session storage namespace
// merging works. Unlike the above test, the swap is for a navigation that would
// normally be cross-process.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNewTabCrossProcess) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));

  // Mock out some URLs and count the number of requests to one of them. Both
  // prerender_session_storage.html and init_session_storage.html need to be
  // mocked so they are same-origin.
  const GURL kInitURL("http://prerender.test/init_session_storage.html");
  base::FilePath init_file = GetTestPath("init_session_storage.html");
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateMockProtocolHandlerOnIO, kInitURL, init_file));

  const GURL kTestURL("http://prerender.test/prerender_session_storage.html");
  base::FilePath test_file = GetTestPath("prerender_session_storage.html");
  RequestCounter counter;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateCountingProtocolHandlerOnIO,
                 kTestURL, test_file, counter.AsWeakPtr()));

  PrerenderTestURL(kTestURL, FINAL_STATUS_USED, 1);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), kInitURL, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Navigate to about:blank so the next navigation is cross-process.
  ui_test_utils::NavigateToURL(current_browser(),
                               GURL(content::kAboutBlankURL));

  // Now navigate in the new tab. Set expect_swap_to_succeed to false because
  // the swap does not occur synchronously.
  //
  // TODO(davidben): When all swaps become asynchronous, remove the OpenURL
  // return value assertion and let this go through the usual successful-swap
  // codepath.
  NavigateToDestURLWithDisposition(CURRENT_TAB, false);

  // Verify DidDisplayPass manually since the previous call skipped it.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));

  // Only one request to the test URL started.
  //
  // TODO(davidben): Re-enable this check when the races in attaching the
  // throttle are resolved. http://crbug.com/335835
  // EXPECT_EQ(1, counter.count());
}

// Verify that session storage conflicts don't merge.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSessionStorageConflict) {
  PrerenderTestURL("files/prerender/prerender_session_storage_conflict.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(),
      test_server()->GetURL("files/prerender/init_session_storage.html"),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Now navigate in the new tab.
  NavigateToDestURLWithDisposition(CURRENT_TAB, false);

  // Verify DidDisplayPass in the new tab.
  EXPECT_TRUE(DidDisplayPass(GetActiveWebContents()));
}

// Checks that prerenders honor |should_replace_current_entry|.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReplaceCurrentEntry) {
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  content::OpenURLParams params(dest_url(), Referrer(), CURRENT_TAB,
                                content::PAGE_TRANSITION_TYPED, false);
  params.should_replace_current_entry = true;
  NavigateToURLWithParams(params, false);

  const NavigationController& controller =
      GetActiveWebContents()->GetController();
  // First entry is about:blank, second is prerender_page.html.
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(GURL(content::kAboutBlankURL),
            controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(dest_url(), controller.GetEntryAtIndex(1)->GetURL());
}

// Checks prerender does not hit DCHECKs and behaves properly if two pending
// swaps occur in a row.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDoublePendingSwap) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  GURL url1 = test_server()->GetURL("files/prerender/prerender_page.html?1");
  scoped_ptr<TestPrerender> prerender1 =
      PrerenderTestURL(url1, FINAL_STATUS_APP_TERMINATING, 1);

  GURL url2 = test_server()->GetURL("files/prerender/prerender_page.html?2");
  scoped_ptr<TestPrerender> prerender2 = ExpectPrerender(FINAL_STATUS_USED);
  AddPrerender(url2, 1);
  prerender2->WaitForStart();
  prerender2->WaitForLoads(1);

  // There's no reason the second prerender can't be used, but the swap races
  // with didStartProvisionalLoad and didFailProvisionalLoad from the previous
  // navigation. The current logic will conservatively fail to swap under such
  // races. However, if the renderer is slow enough, it's possible for the
  // prerender to still be used, so don't program in either expectation.
  ASSERT_TRUE(prerender2->contents());
  prerender2->contents()->set_skip_final_checks(true);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(content::kAboutBlankURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Fire off two navigations, without running the event loop between them.
  NavigationOrSwapObserver swap_observer(
      current_browser()->tab_strip_model(),
      GetActiveWebContents(), 2);
  current_browser()->OpenURL(OpenURLParams(
      url1, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  current_browser()->OpenURL(OpenURLParams(
      url2, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  swap_observer.Wait();

  // The WebContents should be on url2. There may be 2 or 3 entries, depending
  // on whether the first one managed to complete.
  //
  // TODO(davidben): When http://crbug.com/335835 is fixed, the 3 entry case
  // shouldn't be possible because it's throttled by the pending swap that
  // cannot complete.
  const NavigationController& controller =
      GetActiveWebContents()->GetController();
  EXPECT_TRUE(controller.GetPendingEntry() == NULL);
  EXPECT_LE(2, controller.GetEntryCount());
  EXPECT_GE(3, controller.GetEntryCount());
  EXPECT_EQ(GURL(content::kAboutBlankURL),
            controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(
      controller.GetEntryCount() - 1)->GetURL());
}

// Verify that pending swaps get aborted on new navigations.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPendingSwapNewNavigation) {
  PrerenderManager::HangSessionStorageMergesForTesting();

  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(content::kAboutBlankURL), NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Navigate to the URL. Wait for DidStartLoading, just so it's definitely
  // progressed somewhere.
  content::WindowedNotificationObserver page_load_observer(
      content::NOTIFICATION_LOAD_START,
      content::Source<NavigationController>(
          &GetActiveWebContents()->GetController()));
  current_browser()->OpenURL(OpenURLParams(
      dest_url(), Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
  page_load_observer.Wait();

  // Navigate somewhere else. This should succeed and abort the pending swap.
  TestNavigationObserver nav_observer(GetActiveWebContents());
  current_browser()->OpenURL(OpenURLParams(
      GURL(content::kAboutBlankURL), Referrer(), CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED, false));
  nav_observer.Wait();
}

// Checks that <a ping> requests are not dropped in prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPing) {
  // Count hits to a certain URL.
  const GURL kPingURL("http://prerender.test/ping");
  base::FilePath empty_file = ui_test_utils::GetTestFilePath(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("empty.html")));
  RequestCounter ping_counter;
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&CreateCountingProtocolHandlerOnIO,
                 kPingURL, empty_file, ping_counter.AsWeakPtr()));

  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  OpenDestURLViaClickPing(kPingURL);

  ping_counter.WaitForCount(1);
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPPLTNormalNavigation) {
  UMAHistogramHelper histograms;

  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  ui_test_utils::NavigateToURL(current_browser(), url);
  histograms.Fetch();
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLT", 1);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatched", 0);
  histograms.ExpectTotalCount("Prerender.none_PerceivedPLTMatchedComplete", 0);
}

class PrerenderIncognitoBrowserTest : public PrerenderBrowserTest {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    Profile* normal_profile = current_browser()->profile();
    set_browser(ui_test_utils::OpenURLOffTheRecord(
        normal_profile, GURL("about:blank")));
    PrerenderBrowserTest::SetUpOnMainThread();
  }
};

// Checks that prerendering works in incognito mode.
IN_PROC_BROWSER_TEST_F(PrerenderIncognitoBrowserTest, PrerenderIncognito) {
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

}  // namespace prerender
