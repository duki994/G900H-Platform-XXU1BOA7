

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_SBROWSER_CONTENT_BROWSER_CLIENT_DELEGATE_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_SBROWSER_CONTENT_BROWSER_CLIENT_DELEGATE_H_

#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"
#include "base/supports_user_data.h"

namespace content {
class WebContents;
}

namespace chrome {

class SbrowserContentBrowserClientDelegate : public base::SupportsUserData::Data {
  public:
    SbrowserContentBrowserClientDelegate(JNIEnv* env, jobject jdelegate);
    virtual ~SbrowserContentBrowserClientDelegate();

    // Associates the SbrowserContentBrowserClientDelegate with a WebContents using the
    // SupportsUserData mechanism.
    // As implied by the use of scoped_ptr, the WebContents will assume ownership
    // of |delegate|.
    static void Associate(content::WebContents* web_contents,
                          scoped_ptr<SbrowserContentBrowserClientDelegate> delegate);
    // Gets the SbrowserContentBrowserClientDelegate associated with the WebContents,
    // can be null.
    static SbrowserContentBrowserClientDelegate* Get(content::WebContents* web_contents);
    static void SetBingAsCurrentSearchEngine(content::WebContents* source) ;

    virtual void SetBingAsCurrentSearchDefault();
  private:
    JavaObjectWeakGlobalRef weak_jdelegate_;
} ;

bool RegisterSbrowserContentBrowserClientDelegate(JNIEnv* env);
}
#endif  // SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_SBROWSER_CONTENT_BROWSER_CLIENT_DELEGATE_H_
