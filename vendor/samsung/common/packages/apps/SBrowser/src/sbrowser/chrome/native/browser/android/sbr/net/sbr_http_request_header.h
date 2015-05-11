
	 

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NET_SBR_HTTP_REQUEST_HEADER_H
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NET_SBR_HTTP_REQUEST_HEADER_H
#pragma once

#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"

namespace net {
	// JNI registration.
	bool RegisterSbrHttpRequestHeader(JNIEnv* env);
}
#endif //SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NET_SBR_HTTP_REQUEST_HEADER_H

