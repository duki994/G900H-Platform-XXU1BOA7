// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/public/renderer/content_renderer_client.h"
#include "ipc/ipc_channel_proxy.h"

class ChromeRenderProcessObserver;
class PrescientNetworkingDispatcher;
class RendererNetPredictor;
class SearchBouncer;
#if defined(ENABLE_SPELLCHECK)
class SpellCheck;
class SpellCheckProvider;
#endif

struct ChromeViewHostMsg_GetPluginInfo_Output;

namespace content {
struct WebPluginInfo;
}

#if defined(ENABLE_EXTENSIONS_ALL)
namespace extensions {
class Dispatcher;
class Extension;
class ExtensionSet;
class RendererPermissionsPolicyDelegate;
}
#endif

#if defined(ENABLE_PRERENDER)
namespace prerender {
class PrerenderDispatcher;
}
#endif

namespace safe_browsing {
class PhishingClassifierFilter;
}

namespace visitedlink {
class VisitedLinkSlave;
}

namespace blink {
class WebSecurityOrigin;
}

#if defined(ENABLE_WEBRTC)
class WebRtcLoggingMessageFilter;
#endif

class ChromeContentRendererClient : public content::ContentRendererClient {
 public:
  ChromeContentRendererClient();
  virtual ~ChromeContentRendererClient();

  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderFrameCreated(content::RenderFrame* render_frame) OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual void SetNumberOfViews(int number_of_views) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual SkBitmap* GetSadWebViewBitmap() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual bool OverrideCreatePlugin(
      content::RenderFrame* render_frame,
      blink::WebFrame* frame,
      const blink::WebPluginParams& params,
      blink::WebPlugin** plugin) OVERRIDE;
  virtual blink::WebPlugin* CreatePluginReplacement(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path) OVERRIDE;
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain) OVERRIDE;
  virtual bool ShouldSuppressErrorPage(content::RenderFrame* render_frame,
                                       const GURL& url) OVERRIDE;
  virtual void GetNavigationErrorStrings(
      content::RenderView* render_view,
      blink::WebFrame* frame,
      const blink::WebURLRequest& failed_request,
      const blink::WebURLError& error,
      std::string* error_html,
      base::string16* error_description) OVERRIDE;
  virtual void DeferMediaLoad(content::RenderFrame* render_frame,
                              const base::Closure& closure) OVERRIDE;
  virtual bool RunIdleHandlerWhenWidgetsHidden() OVERRIDE;
  virtual bool AllowPopup() OVERRIDE;
  virtual bool ShouldFork(blink::WebFrame* frame,
                          const GURL& url,
                          const std::string& http_method,
                          bool is_initial_navigation,
                          bool is_server_redirect,
                          bool* send_referrer) OVERRIDE;
  virtual bool WillSendRequest(blink::WebFrame* frame,
                               content::PageTransition transition_type,
                               const GURL& url,
                               const GURL& first_party_for_cookies,
                               GURL* new_url) OVERRIDE;
  virtual void DidCreateScriptContext(blink::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) OVERRIDE;
  virtual void WillReleaseScriptContext(blink::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id) OVERRIDE;
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) OVERRIDE;
  virtual bool IsLinkVisited(unsigned long long link_hash) OVERRIDE;
  virtual blink::WebPrescientNetworking* GetPrescientNetworking() OVERRIDE;
  virtual bool ShouldOverridePageVisibilityState(
      const content::RenderFrame* render_frame,
      blink::WebPageVisibilityState* override_state) OVERRIDE;
  virtual bool AllowBrowserPlugin(
      blink::WebPluginContainer* container) OVERRIDE;
  virtual const void* CreatePPAPIInterface(
      const std::string& interface_name) OVERRIDE;
  virtual bool IsExternalPepperPlugin(const std::string& module_name) OVERRIDE;
  virtual blink::WebSpeechSynthesizer* OverrideSpeechSynthesizer(
      blink::WebSpeechSynthesizerClient* client) OVERRIDE;
  virtual bool ShouldReportDetailedMessageForSource(
      const base::string16& source) const OVERRIDE;
  virtual bool ShouldEnableSiteIsolationPolicy() const OVERRIDE;
  virtual blink::WebWorkerPermissionClientProxy*
      CreateWorkerPermissionClientProxy(content::RenderFrame* render_frame,
                                        blink::WebFrame* frame) OVERRIDE;
  virtual bool AllowPepperMediaStreamAPI(const GURL& url) OVERRIDE;
  virtual void AddKeySystems(
      std::vector<content::KeySystemInfo>* key_systems) OVERRIDE;

  // For testing.
#if defined(ENABLE_EXTENSIONS_ALL)
  void SetExtensionDispatcher(extensions::Dispatcher* extension_dispatcher);
#endif

#if defined(ENABLE_SPELLCHECK)
  // Sets a new |spellcheck|. Used for low-mem restart and testing only.
  // Takes ownership of |spellcheck|.
  void SetSpellcheck(SpellCheck* spellcheck);
#endif

  // Called in low-memory conditions to dump the memory used by the spellchecker
  // and start over.
  void OnPurgeMemory();

  static blink::WebPlugin* CreatePlugin(
      content::RenderFrame* render_frame,
      blink::WebFrame* frame,
      const blink::WebPluginParams& params,
      const ChromeViewHostMsg_GetPluginInfo_Output& output);

  static bool IsExtensionOrSharedModuleWhitelisted(
      const GURL& url, const std::set<std::string>& whitelist);

  // TODO(mpcomplete): remove after we collect histogram data.
  // http://crbug.com/100411
  static bool IsAdblockInstalled();
  static bool IsAdblockPlusInstalled();
  static bool IsAdblockWithWebRequestInstalled();
  static bool IsAdblockPlusWithWebRequestInstalled();
  static bool IsOtherExtensionWithWebRequestInstalled();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest, NaClRestriction);
  FRIEND_TEST_ALL_PREFIXES(ChromeContentRendererClientTest,
                           ShouldSuppressErrorPage);

  // Gets extension by the given origin, regardless of whether the extension
  // is active in the current process.
#if defined(ENABLE_EXTENSIONS_ALL)
  const extensions::Extension* GetExtensionByOrigin(
      const blink::WebSecurityOrigin& origin) const;
#endif

  // Returns true if the frame is navigating to an URL either into or out of an
  // extension app's extent.
#if defined(ENABLE_EXTENSIONS_ALL)
  bool CrossesExtensionExtents(blink::WebFrame* frame,
                               const GURL& new_url,
                               const extensions::ExtensionSet& extensions,
                               bool is_extension_url,
                               bool is_initial_navigation);
#endif

  static GURL GetNaClContentHandlerURL(const std::string& actual_mime_type,
                                       const content::WebPluginInfo& plugin);

  // Determines if a NaCl app is allowed, and modifies params to pass the app's
  // permissions to the trusted NaCl plugin.
  static bool IsNaClAllowed(const GURL& manifest_url,
                            const GURL& app_url,
                            bool is_nacl_unrestricted,
#if defined(ENABLE_EXTENSIONS_ALL)
                            const extensions::Extension* extension,
#endif
                            blink::WebPluginParams* params);

  scoped_ptr<ChromeRenderProcessObserver> chrome_observer_;
#if defined(ENABLE_EXTENSIONS_ALL)
  scoped_ptr<extensions::Dispatcher> extension_dispatcher_;
  scoped_ptr<extensions::RendererPermissionsPolicyDelegate>
      permissions_policy_delegate_;
#endif
  scoped_ptr<PrescientNetworkingDispatcher> prescient_networking_dispatcher_;
  scoped_ptr<RendererNetPredictor> net_predictor_;
#if defined(ENABLE_SPELLCHECK)
  scoped_ptr<SpellCheck> spellcheck_;
#endif
  scoped_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;
  scoped_ptr<safe_browsing::PhishingClassifierFilter> phishing_classifier_;
#if defined(ENABLE_PRERENDER)
  scoped_ptr<prerender::PrerenderDispatcher> prerender_dispatcher_;
#endif
#if defined(ENABLE_WEBRTC)
  scoped_refptr<WebRtcLoggingMessageFilter> webrtc_logging_message_filter_;
#endif
  scoped_ptr<SearchBouncer> search_bouncer_;
};

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
