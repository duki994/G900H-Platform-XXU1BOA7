// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SBROWSER_NATIVE_CHROME_BROWSER_BASE_ANDROID_SBR_FEATURE_H_
#define SBROWSER_NATIVE_CHROME_BROWSER_BASE_ANDROID_SBR_FEATURE_H_

#include <jni.h>

#include <string>





namespace base {
	namespace android {
		namespace sbr{
		bool RegisterSbrFeature(JNIEnv* env); 
        bool getEnableStatus( std::string tag );
        std::string getString( std::string tag);
		}
	}
}
	
#endif //SBROWSER_NATIVE_CHROME_BROWSER_BASE_ANDROID_SBR_FEATURE_H_
