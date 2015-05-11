// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
#define CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_

#include <map>
#include <set>

#include "chrome/common/content_settings.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#if defined(ENABLE_EXTENSIONS_ALL)
#include "extensions/common/permissions/api_permission.h"
#endif
#include "third_party/WebKit/public/web/WebPermissionClient.h"

class GURL;

namespace blink {
class WebFrame;
class WebSecurityOrigin;
class WebURL;
}

#if defined(ENABLE_EXTENSIONS_ALL)
namespace extensions {
class Dispatcher;
class Extension;
}
#endif

// Handles blocking content per content settings for each RenderFrame.
class ContentSettingsObserver
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ContentSettingsObserver>,
      public blink::WebPermissionClient {
 public:
#if defined(ENABLE_EXTENSIONS_ALL)
  ContentSettingsObserver(content::RenderFrame* render_frame,
                          extensions::Dispatcher* extension_dispatcher);
#else
  ContentSettingsObserver(content::RenderFrame* render_frame);
#endif
  virtual ~ContentSettingsObserver();

  // Sets the content setting rules which back |AllowImage()|, |AllowScript()|,
  // and |AllowScriptFromSource()|. |content_setting_rules| must outlive this
  // |ContentSettingsObserver|.
  void SetContentSettingRules(
      const RendererContentSettingRules* content_setting_rules);

  bool IsPluginTemporarilyAllowed(const std::string& identifier);

  // Sends an IPC notification that the specified content type was blocked.
  void DidBlockContentType(ContentSettingsType settings_type);

  // blink::WebPermissionClient implementation.
  virtual bool allowDatabase(blink::WebFrame* frame,
                             const blink::WebString& name,
                             const blink::WebString& display_name,
                             unsigned long estimated_size);
  virtual bool allowFileSystem(blink::WebFrame* frame);
  virtual bool allowImage(blink::WebFrame* frame,
                          bool enabled_per_settings,
                          const blink::WebURL& image_url);
  virtual bool allowIndexedDB(blink::WebFrame* frame,
                              const blink::WebString& name,
                              const blink::WebSecurityOrigin& origin);
  virtual bool allowPlugins(blink::WebFrame* frame,
                            bool enabled_per_settings);
  virtual bool allowScript(blink::WebFrame* frame,
                           bool enabled_per_settings);
  virtual bool allowScriptFromSource(blink::WebFrame* frame,
                                     bool enabled_per_settings,
                                     const blink::WebURL& script_url);
  virtual bool allowStorage(blink::WebFrame* frame, bool local);
  virtual bool allowReadFromClipboard(blink::WebFrame* frame,
                                      bool default_value);
  virtual bool allowWriteToClipboard(blink::WebFrame* frame,
                                     bool default_value);
  virtual bool allowWebComponents(blink::WebFrame* frame, bool);
  virtual bool allowMutationEvents(blink::WebFrame* frame,
                                   bool default_value);
  virtual bool allowPushState(blink::WebFrame* frame);
  virtual void didNotAllowPlugins(blink::WebFrame* frame);
  virtual void didNotAllowScript(blink::WebFrame* frame);
  virtual bool allowDisplayingInsecureContent(
      blink::WebFrame* frame,
      bool allowed_per_settings,
      const blink::WebSecurityOrigin& context,
      const blink::WebURL& url);
  virtual bool allowRunningInsecureContent(
      blink::WebFrame* frame,
      bool allowed_per_settings,
      const blink::WebSecurityOrigin& context,
      const blink::WebURL& url);

  // This is used for cases when the NPAPI plugins malfunction if used.
  bool AreNPAPIPluginsBlocked() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsObserverTest, WhitelistedSchemes);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest,
                           ContentSettingsInterstitialPages);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest, PluginsTemporarilyAllowed);

  // RenderFrameObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCommitProvisionalLoad(bool is_new_navigation) OVERRIDE;

  // Message handlers.
  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetAsInterstitial();
  void OnNPAPINotSupported();
  void OnSetAllowDisplayingInsecureContent(bool allow);
  void OnSetAllowRunningInsecureContent(bool allow);
  void OnReloadFrame();

  // Resets the |content_blocked_| array.
  void ClearBlockedContentSettings();

#if defined(ENABLE_EXTENSIONS_ALL)
  // If |origin| corresponds to an installed extension, returns that extension.
  // Otherwise returns NULL.
  const extensions::Extension* GetExtension(
      const blink::WebSecurityOrigin& origin) const;
#endif

  // Helpers.
  // True if |frame| contains content that is white-listed for content settings.
  static bool IsWhitelistedForContentSettings(blink::WebFrame* frame);
  static bool IsWhitelistedForContentSettings(
      const blink::WebSecurityOrigin& origin,
      const GURL& document_url);

#if defined(ENABLE_EXTENSIONS_ALL)
  // Owned by ChromeContentRendererClient and outlive us.
  extensions::Dispatcher* extension_dispatcher_;
#endif

  // Insecure content may be permitted for the duration of this render view.
  bool allow_displaying_insecure_content_;
  bool allow_running_insecure_content_;

  // A pointer to content setting rules stored by the renderer. Normally, the
  // |RendererContentSettingRules| object is owned by
  // |ChromeRenderProcessObserver|. In the tests it is owned by the caller of
  // |SetContentSettingRules|.
  const RendererContentSettingRules* content_setting_rules_;

  // Stores if images, scripts, and plugins have actually been blocked.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Caches the result of AllowStorage.
  typedef std::pair<GURL, bool> StoragePermissionsKey;
  std::map<StoragePermissionsKey, bool> cached_storage_permissions_;

  // Caches the result of |AllowScript|.
  std::map<blink::WebFrame*, bool> cached_script_permissions_;

  std::set<std::string> temporarily_allowed_plugins_;
  bool is_interstitial_page_;
  bool npapi_plugins_blocked_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsObserver);
};

#endif  // CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
