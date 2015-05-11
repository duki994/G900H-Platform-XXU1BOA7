

#ifndef CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_

#include <jni.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/tab_android.h"

class SbrTabBridge {
public:
  SbrTabBridge(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, jobject obj);
#if defined(ENABLE_MOSTVISITED)
  jboolean IsURLHasThumbnail(JNIEnv* env,jobject obj,jstring jurl);
#endif
  // Extra API required for Application
  base::android::ScopedJavaLocalRef<jbyteArray> GetStateAsByteArray (JNIEnv* env,
                                    jobject obj,
                                    jobject jcontent_view_core);
  void CreateHistoricalTab(JNIEnv* env, jobject obj,
                          jobject jcontent_view_core,jint tab_index);
  bool  IsInitialNavigation(JNIEnv* env, jobject obj,jobject jcontent_view_core);
  int GetRenderProcessPrivateSizeKBytes ( JNIEnv* env, jobject obj,jobject jcontent_view_core) ;
  void PurgeRenderProcessNativeMemory ( JNIEnv* env, jobject obj,jobject jcontent_view_core );
  void SetInterceptNavigationDelegate ( JNIEnv* env,
                              jobject obj,
                              jobject intercept_navigation_delegate,
                              jobject jcontent_view_core);
  void SetSbrowserContentBrowserClientDelegate(JNIEnv* env,
                              jobject obj,
                              jobject sbrowser_content_browser_client_delegate,
                              jobject jcontent_view_core);
  int GetRenderProcessPid ( JNIEnv* env, jobject obj,jobject jcontent_view_core) ;

  // Register the Tab's native methods through JNI.
  static bool RegisterSbrTabBridge(JNIEnv* env);

  
  void StartFinding(JNIEnv* env, jobject obj,jobject jcontent_view_core,
                    jstring search_string,
                    jboolean forward_direction,
                    jboolean case_sensitive);
  void StopFinding (JNIEnv* env, jobject obj,
                    jobject jcontent_view_core, jint selection_action);
  void RequestFindMatchRects(JNIEnv* env, jobject obj,
                            jobject jcontent_view_core, jint current_version);
  base::android::ScopedJavaLocalRef<jstring> GetPreviousFindText(JNIEnv* env, jobject obj,
                                    jobject jcontent_view_core);
  void ActivateNearestFindResult(JNIEnv* env, jobject obj,
                                jobject jcontent_view_core, float x, float y);

  void UpdateThumbnailWithOriginalURL(JNIEnv* env, jobject obj,
            jobject jcontent_view_core, jstring jurl, jobject jbitmap);

#if defined(ENABLE_MOSTVISITED)
  void RemoveBlacklistURL(JNIEnv* env, jobject obj, jstring jurl);
#endif

  enum {
    keepSelectionOnPage = 0,
    clearSelectionOnPage = 1,
    activateSelectionOnPage = 2
  };
  

protected:
  virtual ~SbrTabBridge();

private:
  DISALLOW_COPY_AND_ASSIGN(SbrTabBridge);
};

#endif  // CHROME_BROWSER_ANDROID_SBR_TAB_SBR_TAB_BRIDGE_H_
