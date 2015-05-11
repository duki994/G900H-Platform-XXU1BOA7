

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_AUTOCOMPLETE_AUTOCOMPLETE_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_AUTOCOMPLETE_AUTOCOMPLETE_H_

#include <string>

#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

class AutocompleteController;
class AutocompleteResult;
class Profile;


// The native part of the Java AutocompleteBridge class.
class SbrAutocompleteBridge : public AutocompleteControllerDelegate {
 public:

  static const int kDefaultOmniboxProviders;

  SbrAutocompleteBridge(JNIEnv* env, jobject obj, Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);

  // Registers the LocationBar native method.
  static bool RegisterSbrAutocompleteBridge(JNIEnv* env);

  // Autocomplete method
  void Start(JNIEnv* env,
             jobject obj,
             jstring text,
             jstring desired_tld,
             jstring current_url,
             bool prevent_inline_autocomplete,
             bool prefer_keyword,
             bool allow_exact_keyword_match,
             bool synchronous_only);

  // Autocomplete method
  void Stop(JNIEnv* env, jobject obj, bool clear_result);

  // Attempts to fully qualify an URL from an inputed search query, |jquery|.
  // If the query does not appear to be a URL, this will return NULL.
  static jstring QualifyPartialURLQuery(
      JNIEnv* env, jclass clazz, jstring jquery);

 private:
  virtual ~SbrAutocompleteBridge();
  void InitJNI(JNIEnv* env, jobject obj);

  // AutocompleteControllerDelegate method:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  // Notifies the Java LocationBar that suggestions were received based on the
  // text the user typed in last.
  void NotifySuggestionsReceived(
      const AutocompleteResult& autocomplete_result);

  // A weak ref to the profile associated with |autocomplete_controller_|.
  Profile* profile_;

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  JavaObjectWeakGlobalRef weak_java_autocomplete_bridge_;

  base::android::ScopedJavaGlobalRef<jclass> omnibox_suggestion_class_;

  jmethodID omnibox_suggestion_constructor_;

  DISALLOW_COPY_AND_ASSIGN(SbrAutocompleteBridge);
};

#endif //SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_AUTOCOMPLETE_AUTOCOMPLETE_H_
