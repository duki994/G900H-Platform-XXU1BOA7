

#ifndef SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_CONTENT_SETTINGS_H_
#define SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_CONTENT_SETTINGS_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class SbrContentSettings : public WebContentsObserver {

 public:
  SbrContentSettings(JNIEnv* env, jobject obj,
                     WebContents* contents);
  // Synchronizes the Java settings from native settings.
  void SyncFromNative(JNIEnv* env, jobject obj);

  // Synchronizes the native settings from Java settings.
  void SyncToNative(JNIEnv* env, jobject obj);
  //void SetWebContents(JNIEnv* env, jobject obj, jint web_contents);

 private:
  struct FieldIds;

  // Self-deletes when the underlying WebContents is destroyed.
  virtual ~SbrContentSettings();

  // WebContentsObserver overrides:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

  void SyncToNativeImpl();
  void SyncFromNativeImpl();

  // Java field references for accessing the values in the Java object.
  scoped_ptr<FieldIds> field_ids_;

  // The Java counterpart to this class.
  JavaObjectWeakGlobalRef sbr_content_settings_;
};

bool RegisterSbrContentSettings(JNIEnv* env);

}  // namespace content

#endif  // SBR_NATIVE_CONTENT_BROWSER_ANDROID_SBR_SBR_CONTENT_SETTINGS_H_
