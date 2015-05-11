

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_LOCATIONBAR_SBR_LOCATION_BAR_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_LOCATIONBAR_SBR_LOCATION_BAR_H_

#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/android/jni_helper.h"

class Profile;

// The native part of the Java SbrLocationBar class.
// Note that there should only be one instance of this class whose lifecycle
// is managed from the Java side.
class SbrLocationBar {
public:

  SbrLocationBar(JNIEnv* env, jobject obj, Profile* profile);

  void Destroy(JNIEnv* env, jobject obj);

  // Called by the Java code when the user clicked on the security button.
  void OnSecurityButtonClicked(JNIEnv* env, jobject, jobject context, jobject contentview);

private:
  virtual ~SbrLocationBar();

  JavaObjectWeakGlobalRef weak_java_location_bar_;

  DISALLOW_COPY_AND_ASSIGN(SbrLocationBar);
};

// Registers the SbrLocationBar native method.
bool RegisterSbrLocationBar(JNIEnv* env);

#endif  // SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_LOCATIONBAR_SBR_LOCATION_BAR_H_
