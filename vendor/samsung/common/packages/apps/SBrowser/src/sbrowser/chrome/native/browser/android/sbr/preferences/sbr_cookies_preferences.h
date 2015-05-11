

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_COOKIES_PREFERENCES_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_COOKIES_PREFERENCES_H_
#pragma once

#include "base/android/jni_android.h"

void SbrGetCurrentCookieCount(JNIEnv* env, jobject obj);
void SbrGetCookiesForUrl(JNIEnv* env, jobject obj, jstring url);
void SbrGetOffTheRecordCookiesForUrl(JNIEnv* env, jobject obj, jstring url);
void sbrGetCurrentCacheSize(JNIEnv* env, jobject obj);
void SbrFlushCookieStore();

#endif // SBROWSER_NATIVE_CHROME_BROWSER_ANDROID_SBR_PREFERENCES_SBR_COOKIES_PREFERENCES_H_

