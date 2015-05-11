// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.sec.chromium.content.browser;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;

import java.util.ArrayList;
import java.util.List;


// This is the implementation of the C++ counterpart PushProvider.
@JNINamespace("content")
class PushProvider {
    private static final String TAG = "PushProvider";
    private long mNativePushProviderAndroid;

    String removeSlashFromOrigin(String origin) {
        if (origin.charAt(origin.length()-1) != '/')
            return origin;

        Log.e(TAG, "registerPush : " + origin.substring(0, origin.length()-1));
        return origin.substring(0, origin.length()-1);
    }

    @CalledByNative
    static PushProvider create(long nativePushProviderAndroid) {
        return new PushProvider(nativePushProviderAndroid);
    }

    @CalledByNative
    void register(final Context context, String origin, long requestId) {
      
    }

    @CalledByNative
    void unregister(final Context context, String origin, long requestId) {
     
    }

    @CalledByNative
    boolean isRegistered(final Context context, String origin) {
     return false;
    }

    private PushProvider(long nativePushProviderAndroid) {
        mNativePushProviderAndroid = nativePushProviderAndroid;
    }

  

    
}
