
	 

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NET_SBR_URL_FILTER_H
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_NET_SBR_URL_FILTER_H
#pragma once

namespace net {
bool SbrUrlFilterIsBlockedUrl(const std::string& url);

bool RegisterSbrUrlFilter(JNIEnv* env);
}

#endif