// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;

/**
 * Manages settings state for a ContentView. A ContentSettings instance is obtained
 * from ContentViewCore.getContentSettings().
 */
@JNINamespace("content")
public class ContentSettings {

    private static final String TAG = "ContentSettings";

    // The native side of this object. Ownership is retained native-side by the WebContents
    // instance that backs the associated ContentViewCore.
    private long mNativeContentSettings = 0;

    private ContentViewCore mContentViewCore;

    /**
     * Package constructor to prevent clients from creating a new settings
     * instance. Must be called on the UI thread.
     */
    ContentSettings(ContentViewCore contentViewCore, long nativeContentView) {
        ThreadUtils.assertOnUiThread();
        mContentViewCore = contentViewCore;
        mNativeContentSettings = nativeInit(nativeContentView);
        assert mNativeContentSettings != 0;
    }

    /**
     * Notification from the native side that it is being destroyed.
     * @param nativeContentSettings the native instance that is going away.
     */
    @CalledByNative
    private void onNativeContentSettingsDestroyed(int nativeContentSettings) {
        assert mNativeContentSettings == nativeContentSettings;
        mNativeContentSettings = 0;
    }

    /**
     * Return true if JavaScript is enabled. Must be called on the UI thread.
     *
     * @return True if JavaScript is enabled.
     */
    public boolean getJavaScriptEnabled() {
        ThreadUtils.assertOnUiThread();
        return mNativeContentSettings != 0 ?
                nativeGetJavaScriptEnabled(mNativeContentSettings) : false;
    }

    //SBROWSER_MULTIINSTANCE_TAB_DRAG_N_DROP >>
    /**
    * Destroys the native side of this object as this will trigger detaching
    * from the web contents.
    * Required to be called during drag and drop when associated
    * ContentViewCore is being destroyed.
    * Strictly to be used only for Multi-Instance TAB Drag & Drop
    */
    public void destroyNative() {
        if (mNativeContentSettings != 0) {
            // FIXME This scenario will be covered as fixup patch
            //nativeDestroy(mNativeContentSettings);
        }
    }
    //SBROWSER_MULTIINSTANCE_TAB_DRAG_N_DROP <<

    // Initialize the ContentSettings native side.
    private native long nativeInit(long contentViewPtr);

    private native boolean nativeGetJavaScriptEnabled(long nativeContentSettings);
}
