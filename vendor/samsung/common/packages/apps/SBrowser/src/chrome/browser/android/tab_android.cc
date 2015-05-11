// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/debug/trace_event.h"
#include "chrome/browser/android/chrome_web_contents_delegate_android.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/printing/print_view_manager_basic.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#if defined(ENABLE_SYNC)
#include "chrome/browser/sync/glue/synced_tab_delegate_android.h"
#endif
#include "chrome/browser/ui/android/content_settings/popup_blocked_infobar_delegate.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/android/infobars/infobar_container_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabBase_jni.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/android/device_display_info.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

#if defined(S_NATIVE_SUPPORT)
#if defined(ENABLE_EXTENSIONS_ALL)  
#include "chrome/browser/extensions/tab_helper.h"
#endif
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/web_contents_view.h"

using content::GlobalRequestID;
using content::NavigationController;
using content::WebContents;
#endif

using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF16ToJavaString;

TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  CoreTabHelper* core_tab_helper = CoreTabHelper::FromWebContents(web_contents);
  if (!core_tab_helper)
    return NULL;

  CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
  if (!core_delegate)
    return NULL;

  return static_cast<TabAndroid*>(core_delegate);
}

TabAndroid* TabAndroid::GetNativeTab(JNIEnv* env, jobject obj) {
  return reinterpret_cast<TabAndroid*>(Java_TabBase_getNativePtr(env, obj));
}

TabAndroid::TabAndroid(JNIEnv* env, jobject obj)
    : weak_java_tab_(env, obj),
#if defined(ENABLE_SYNC)
      session_tab_id_(),
      synced_tab_delegate_(new browser_sync::SyncedTabDelegateAndroid(this)) {
#else
      session_tab_id_() {
#endif
  Java_TabBase_setNativePtr(env, obj, reinterpret_cast<intptr_t>(this));
}

TabAndroid::~TabAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return;

  Java_TabBase_clearNativePtr(env, obj.obj());
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return weak_java_tab_.get(env);
}

int TabAndroid::GetAndroidId() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return -1;
  return Java_TabBase_getId(env, obj.obj());
}

int TabAndroid::GetSyncId() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return 0;
  return Java_TabBase_getSyncId(env, obj.obj());
}

base::string16 TabAndroid::GetTitle() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return base::string16();
  return base::android::ConvertJavaStringToUTF16(
      Java_TabBase_getTitle(env, obj.obj()));
}

GURL TabAndroid::GetURL() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return GURL::EmptyGURL();
  return GURL(base::android::ConvertJavaStringToUTF8(
      Java_TabBase_getUrl(env, obj.obj())));
}

bool TabAndroid::RestoreIfNeeded() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return false;
  return Java_TabBase_restoreIfNeeded(env, obj.obj());
}

content::ContentViewCore* TabAndroid::GetContentViewCore() const {
  if (!web_contents())
    return NULL;

  return content::ContentViewCore::FromWebContents(web_contents());
}

Profile* TabAndroid::GetProfile() const {
  if (!web_contents())
    return NULL;

  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

#if defined(ENABLE_SYNC)
browser_sync::SyncedTabDelegate* TabAndroid::GetSyncedTabDelegate() const {
  return synced_tab_delegate_.get();
}
#endif

void TabAndroid::SetSyncId(int sync_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return;
  Java_TabBase_setSyncId(env, obj.obj(), sync_id);
}

void TabAndroid::HandlePopupNavigation(chrome::NavigateParams* params) {
#if defined(S_NATIVE_SUPPORT)
  if (!params->url.is_empty()) {
    bool was_blocked = false;
    GURL url = params->url;
    params->target_contents = SbrCreateTargetContents(*params, url);
    SbrLoadURLInContents(params->target_contents, url, params);
    web_contents_delegate_->AddNewContents(
            params->source_contents,
            params->target_contents,
            params->disposition,
            params->window_bounds,
            params->user_gesture,
            &was_blocked);
    if(was_blocked)
      params->target_contents = NULL;
  }
#else
  NOTIMPLEMENTED();
#endif
}

void TabAndroid::OnReceivedHttpAuthRequest(jobject auth_handler,
                                           const base::string16& host,
                                           const base::string16& realm) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_host = ConvertUTF16ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> jstring_realm = ConvertUTF16ToJavaString(env, realm);

  Java_TabBase_OnReceivedHttpAuthRequest(
      env,
      weak_java_tab_.get(env).obj(),
      auth_handler,
      jstring_host.obj(),
      jstring_realm.obj());
}

void TabAndroid::AddShortcutToBookmark(const GURL& url,
                                       const base::string16& title,
                                       const SkBitmap& skbitmap,
                                       int r_value,
                                       int g_value,
                                       int b_value) {
  NOTREACHED();
}

void TabAndroid::EditBookmark(int64 node_id,
                              const base::string16& node_title,
                              bool is_folder,
                              bool is_partner_bookmark) {
  NOTREACHED();
}

void TabAndroid::OnNewTabPageReady() {
  NOTREACHED();
}

bool TabAndroid::ShouldWelcomePageLinkToTermsOfService() {
  NOTIMPLEMENTED();
  return false;
}

void TabAndroid::SwapTabContents(content::WebContents* old_contents,
                                 content::WebContents* new_contents,
                                 bool did_start_load,
                                 bool did_finish_load) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // We need to notify the native InfobarContainer so infobars can be swapped.
  InfoBarContainerAndroid* infobar_container =
      reinterpret_cast<InfoBarContainerAndroid*>(
          Java_TabBase_getNativeInfoBarContainer(
              env,
              weak_java_tab_.get(env).obj()));
  InfoBarService* new_infobar_service = new_contents ?
      InfoBarService::FromWebContents(new_contents) : NULL;
  infobar_container->ChangeInfoBarService(new_infobar_service);

  Java_TabBase_swapWebContents(
      env,
      weak_java_tab_.get(env).obj(),
      reinterpret_cast<intptr_t>(new_contents),
      did_start_load,
      did_finish_load);
}

void TabAndroid::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_tab_.get(env);
  if (obj.is_null())
    return;

  switch (type) {
    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      TabSpecificContentSettings* settings =
          TabSpecificContentSettings::FromWebContents(web_contents());
      if (!settings->IsBlockageIndicated(CONTENT_SETTINGS_TYPE_POPUPS)) {
        // TODO(dfalcantara): Create an InfoBarDelegate to keep the
        // PopupBlockedInfoBar logic native-side instead of straddling the JNI
        // boundary.
        int num_popups = 0;
        PopupBlockerTabHelper* popup_blocker_helper =
            PopupBlockerTabHelper::FromWebContents(web_contents());
        if (popup_blocker_helper)
          num_popups = popup_blocker_helper->GetBlockedPopupsCount();

        if (num_popups > 0) {
          PopupBlockedInfoBarDelegate::Create(web_contents(), num_popups);
          Java_TabBase_onPopupBlockStateChanged(env, obj.obj());
        }

        settings->SetBlockageHasBeenIndicated(CONTENT_SETTINGS_TYPE_POPUPS);
      }
      break;
    }
    case chrome::NOTIFICATION_FAVICON_UPDATED:
      Java_TabBase_onFaviconUpdated(env, obj.obj());
      break;
    case chrome::NOTIFICATION_TOUCHICON_UPDATED:
      Java_TabBase_onTouchiconUpdated(env, obj.obj());
      break;
    case content::NOTIFICATION_NAV_ENTRY_CHANGED:
      Java_TabBase_onNavEntryChanged(env, obj.obj());
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
      break;
  }
}

void TabAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void TabAndroid::InitWebContents(JNIEnv* env,
                                 jobject obj,
                                 jboolean incognito,
                                 jobject jcontent_view_core,
                                 jobject jweb_contents_delegate,
                                 jobject jcontext_menu_populator) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  DCHECK(content_view_core);
  DCHECK(content_view_core->GetWebContents());

  web_contents_.reset(content_view_core->GetWebContents());
  TabHelpers::AttachTabHelpers(web_contents_.get());

  session_tab_id_.set_id(
      SessionTabHelper::FromWebContents(web_contents())->session_id().id());
  ContextMenuHelper::FromWebContents(web_contents())->SetPopulator(
      jcontext_menu_populator);
  WindowAndroidHelper::FromWebContents(web_contents())->
      SetWindowAndroid(content_view_core->GetWindowAndroid());
  CoreTabHelper::FromWebContents(web_contents())->set_delegate(this);
  web_contents_delegate_.reset(
      new chrome::android::ChromeWebContentsDelegateAndroid(
          env, jweb_contents_delegate));
  web_contents_delegate_->LoadProgressChanged(web_contents(), 0);
  web_contents()->SetDelegate(web_contents_delegate_.get());

  LOG(INFO) << __FUNCTION__ << " setting web contents = " << web_contents();

  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      chrome::NOTIFICATION_TOUCHICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_CHANGED,
      content::Source<content::NavigationController>(
           &web_contents()->GetController()));

#if defined(ENABLE_SYNC)
  synced_tab_delegate_->SetWebContents(web_contents());
#endif

  // Set the window ID if there is a valid TabModel.
  TabModel* model = TabModelList::GetTabModelWithProfile(GetProfile());
  if (model) {
    SessionID window_id;
    window_id.set_id(model->GetSessionId());

    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(web_contents());
    session_tab_helper->SetWindowID(window_id);
  }

  // Verify that the WebContents this tab represents matches the expected
  // off the record state.
  CHECK_EQ(GetProfile()->IsOffTheRecord(), incognito);
}

void TabAndroid::DestroyWebContents(JNIEnv* env,
                                    jobject obj,
                                    jboolean delete_native) {
  DCHECK(web_contents());

  if(!web_contents())
    LOG(ERROR) << " web contents should not be NULL";

  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));
  notification_registrar_.Remove(
      this,
      chrome::NOTIFICATION_TOUCHICON_UPDATED,
      content::Source<content::WebContents>(web_contents()));

  web_contents()->SetDelegate(NULL);

  if (delete_native) {
    LOG(INFO) << __FUNCTION__ << " web contents resetting..." << web_contents();
    web_contents_.reset();
#if defined(ENABLE_SYNC)
    synced_tab_delegate_->ResetWebContents();
#endif
    LOG(INFO) << __FUNCTION__ << " web contents reseted..." << web_contents();
  } else {
    LOG(INFO) << __FUNCTION__ << " web contents releasing..." << web_contents();
    // Release the WebContents so it does not get deleted by the scoped_ptr.
    ignore_result(web_contents_.release());
  }
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetWebContents(
    JNIEnv* env,
    jobject obj) {
  if (!web_contents_.get())
    return base::android::ScopedJavaLocalRef<jobject>();
  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject> TabAndroid::GetProfileAndroid(
    JNIEnv* env,
    jobject obj) {
  Profile* profile = GetProfile();
  if (!profile)
    return base::android::ScopedJavaLocalRef<jobject>();
  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (!profile_android)
    return base::android::ScopedJavaLocalRef<jobject>();

  return profile_android->GetJavaObject();
}

ToolbarModel::SecurityLevel TabAndroid::GetSecurityLevel(JNIEnv* env,
                                                         jobject obj) {
  return ToolbarModelImpl::GetSecurityLevelForWebContents(web_contents());
}

void TabAndroid::SetActiveNavigationEntryTitleForUrl(JNIEnv* env,
                                                     jobject obj,
                                                     jstring jurl,
                                                     jstring jtitle) {
  DCHECK(web_contents());

  base::string16 title;
  if (jtitle)
    title = base::android::ConvertJavaStringToUTF16(env, jtitle);

  std::string url;
  if (jurl)
    url = base::android::ConvertJavaStringToUTF8(env, jurl);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (entry && url == entry->GetVirtualURL().spec())
    entry->SetTitle(title);
}

bool TabAndroid::Print(JNIEnv* env, jobject obj) {
#if defined(ENABLE_PRINTING)
  if (!web_contents())
    return false;

  printing::PrintViewManagerBasic::CreateForWebContents(web_contents());
  printing::PrintViewManagerBasic* print_view_manager =
      printing::PrintViewManagerBasic::FromWebContents(web_contents());
  if (print_view_manager == NULL)
    return false;

  print_view_manager->PrintNow();
  return true;
#else
  return false;
#endif
}

ScopedJavaLocalRef<jobject> TabAndroid::GetFavicon(JNIEnv* env, jobject jobj) {
  ScopedJavaLocalRef<jobject> bitmap;
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(web_contents_.get());

  if (!favicon_tab_helper)
    return bitmap;
  if (!favicon_tab_helper->FaviconIsValid())
    return bitmap;

  SkBitmap favicon =
      favicon_tab_helper->GetFavicon()
          .AsImageSkia()
          .GetRepresentation(
               ResourceBundle::GetSharedInstance().GetMaxScaleFactor())
          .sk_bitmap();

  if (favicon.empty()) {
    favicon = favicon_tab_helper->GetFavicon().AsBitmap();
  }

  if (!favicon.empty()) {
    gfx::DeviceDisplayInfo device_info;
    const float device_scale_factor = device_info.GetDIPScale();
    int target_size_dip = device_scale_factor * gfx::kFaviconSize;
    if (favicon.width() != target_size_dip ||
        favicon.height() != target_size_dip) {
      favicon =
          skia::ImageOperations::Resize(favicon,
                                        skia::ImageOperations::RESIZE_BEST,
                                        target_size_dip,
                                        target_size_dip);
    }

    bitmap = gfx::ConvertToJavaBitmap(&favicon);
  }
  return bitmap;
}

// SBROWSER_MULTIINSTANCE_TAB_DRAG_N_DROP <<
long TabAndroid::GetWebContentsPtr(JNIEnv* env, jobject obj) {
    if(web_contents_) {
        return reinterpret_cast<long>(web_contents_.get());
    }
    else
        return 0;
}
// SBROWSER_MULTIINSTANCE_TAB_DRAG_N_DROP>>

#if defined(S_NATIVE_SUPPORT)
WebContents* TabAndroid::SbrCreateTargetContents(const chrome::NavigateParams& params,
                                           const GURL& url) {
  WebContents::CreateParams create_params(
      params.initiating_profile,
      tab_util::GetSiteInstanceForNewTab(params.initiating_profile, url));
  if (params.source_contents) {
    create_params.initial_size =
        params.source_contents->GetView()->GetContainerSize();
    if (params.should_set_opener)
      create_params.opener = params.source_contents;
  }
  if (params.disposition == NEW_BACKGROUND_TAB)
    create_params.initially_hidden = true;

  WebContents* target_contents = WebContents::Create(create_params);

  // New tabs can have WebUI URLs that will make calls back to arbitrary
  // tab helpers, so the entire set of tab helpers needs to be set up
  // immediately.
  TabHelpers::AttachTabHelpers(target_contents);
#if defined(ENABLE_EXTENSIONS_ALL)  
  extensions::TabHelper::FromWebContents(target_contents)->
      SetExtensionAppById(params.extension_app_id);
#endif  
  return target_contents;
}

void TabAndroid::SbrLoadURLInContents(content::WebContents* target_contents,
                       const GURL& url,
                       chrome::NavigateParams* params) {
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.referrer = params->referrer;
  load_url_params.frame_tree_node_id = params->frame_tree_node_id;
  load_url_params.redirect_chain = params->redirect_chain;
  load_url_params.transition_type = params->transition;
  load_url_params.extra_headers = params->extra_headers;
  load_url_params.should_replace_current_entry =
      params->should_replace_current_entry;

  if (params->transferred_global_request_id != GlobalRequestID()) {
    load_url_params.is_renderer_initiated = params->is_renderer_initiated;
    load_url_params.transferred_global_request_id =
        params->transferred_global_request_id;
  } else if (params->is_renderer_initiated) {
    load_url_params.is_renderer_initiated = true;
  }

  // Only allows the browser-initiated navigation to use POST.
  if (params->uses_post && !params->is_renderer_initiated) {
    load_url_params.load_type =
        NavigationController::LOAD_TYPE_BROWSER_INITIATED_HTTP_POST;
    load_url_params.browser_initiated_post_data =
        params->browser_initiated_post_data;
  }
  target_contents->GetController().LoadURLWithParams(load_url_params);
}
#endif

static void Init(JNIEnv* env, jobject obj) {
  TRACE_EVENT0("native", "TabAndroid::Init");
  // This will automatically bind to the Java object and pass ownership there.
  new TabAndroid(env, obj);
}

bool TabAndroid::RegisterTabAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
