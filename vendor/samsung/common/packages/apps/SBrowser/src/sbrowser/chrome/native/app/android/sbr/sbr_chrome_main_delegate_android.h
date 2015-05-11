


#ifndef SBR_CHROME_MAIN_DELEGATE_ANDROID_H_
#define SBR_CHROME_MAIN_DELEGATE_ANDROID_H_

#include "chrome/app/android/chrome_main_delegate_android.h"

class SbrChromeMainDelegateAndroid : public ChromeMainDelegateAndroid {
public:
  SbrChromeMainDelegateAndroid();
  virtual ~SbrChromeMainDelegateAndroid();

  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;

  virtual bool RegisterApplicationNativeMethods(JNIEnv* env) OVERRIDE;

private:
  DISALLOW_COPY_AND_ASSIGN(SbrChromeMainDelegateAndroid);
};

#endif  // SBR_CHROME_MAIN_DELEGATE_ANDROID_H_
