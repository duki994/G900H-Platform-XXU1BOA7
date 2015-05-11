// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.util.Log;

/**
 * This class provides a way to create the native WebContents required for instantiating a
 * ContentView.
 */
public abstract class ContentViewUtil {
	
	private static final String LOGTAG = ContentViewUtil.class.getSimpleName();
    // Don't instantiate me.
    private ContentViewUtil() {
    }

    /**
     * @return pointer to native WebContents instance, suitable for using with a
     *         (java) ContentViewCore instance.
     */
    public static long createNativeWebContents(boolean incognito) {
    	//Debug logs needed for analyzing once dlfree issue (P140602-00938) 
    	long nativeWebContents = nativeCreateNativeWebContents(incognito, false);
    	Log.d(LOGTAG,"createNativeWebContents returns nativeWebContents = "+nativeWebContents);
        return nativeWebContents;
    }

    /**
     * @return pointer to native WebContents instance, suitable for using with a
     *         (java) ContentViewCore instance.
     */
    public static long createNativeWebContents(boolean incognito, boolean initiallyHidden) {
    	//Debug logs needed for analyzing once dlfree issue (P140602-00938) 
    	long nativeWebContents = nativeCreateNativeWebContents(incognito, initiallyHidden);
    	Log.d(LOGTAG,"createNativeWebContents returns nativeWebContents= "+nativeWebContents);
        return nativeWebContents;
    }

    /**
     * @param webContentsPtr The WebContents reference to be deleted.
     */
    public static void destroyNativeWebContents(long webContentsPtr) {
    	//Debug logs needed for analyzing once dlfree issue (P140602-00938) 
    	Log.d(LOGTAG,"destroyNativeWebContents webContentsPtr= "+webContentsPtr);
        nativeDestroyNativeWebContents(webContentsPtr);
    }

    private static native long nativeCreateNativeWebContents(boolean incognito,
            boolean initiallyHidden);
    private static native void nativeDestroyNativeWebContents(long webContentsPtr);
}
