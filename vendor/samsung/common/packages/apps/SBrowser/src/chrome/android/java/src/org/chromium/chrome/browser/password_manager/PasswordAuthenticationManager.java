// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import android.util.Log;

import org.chromium.base.CalledByNative;
import org.chromium.chrome.browser.TabBase;

/**
 * Allows embedders to authenticate the usage of passwords.
 */
public class PasswordAuthenticationManager {
    private static final String TAG = PasswordAuthenticationManager.class
            .getSimpleName();

    /**
     * The delegate that allows embedders to control the authentication of passwords.
     */
    public interface PasswordAuthenticationDelegate {
        /**
         * @return Whether password authentication is enabled.
         */
        boolean isPasswordAuthenticationEnabled();

        /**
         * Requests password authentication be presented for the given tab.
         * @param tab The tab containing the protected password.
         * @param callback The callback to be triggered on authentication result.
         */
        void requestAuthentication(TabBase tab,
                PasswordAuthenticationCallback callback, String[] usernames,
                boolean hideMultiUserScreen);

        /**
         * @return The message to be displayed in the save password infobar that will allow
         *         the user to opt-in to additional password authentication.
         */
        String getPasswordProtectionString();

        void showAutoCompleteAlertPopUp();

        boolean isWebSignInEnabled();
    }

    /**
     * The callback to be triggered on success or failure of the password authentication.
     */
    public static class PasswordAuthenticationCallback {
        private long mNativePtr;

        @CalledByNative("PasswordAuthenticationCallback")
        private static PasswordAuthenticationCallback create(long nativePtr) {
            return new PasswordAuthenticationCallback(nativePtr);
        }

        private PasswordAuthenticationCallback(long nativePtr) {
            mNativePtr = nativePtr;
        }

        /**
         * Called upon authentication results to allow usage of the password or not.
         * @param authenticated Whether the authentication was successful.
         */
        public final void onResult(boolean authenticated, String selectedAcount) {
            if (mNativePtr == 0) {
                assert false : "Can not call onResult more than once per callback.";
                return;
            }
            nativeOnResult(mNativePtr, authenticated,selectedAcount);
            mNativePtr = 0;
        }
    }

    private static class DefaultPasswordAuthenticationDelegate
            implements PasswordAuthenticationDelegate {
        @Override
        public boolean isPasswordAuthenticationEnabled() {
            return false;
        }

        @Override
        public void requestAuthentication(TabBase tab,
                PasswordAuthenticationCallback callback, String[] usernames,
                boolean hideMultiUserScreen) {
            callback.onResult(true, "");
        }

        @Override
        public String getPasswordProtectionString() {
            return "";
        }

        @Override
        public void showAutoCompleteAlertPopUp() {
            // TODO Auto-generated method stub
        }

        @Override
        public boolean isWebSignInEnabled() {
            // TODO Auto-generated method stub
            return false;
        }

    }

    private static PasswordAuthenticationDelegate sDelegate;

    private PasswordAuthenticationManager() {}

    private static PasswordAuthenticationDelegate getDelegate() {
        if (sDelegate == null) {
            sDelegate = new DefaultPasswordAuthenticationDelegate();
        }
        return sDelegate;
    }

    /**
     * Sets the password authentication delegate to be used.
     */
    public static void setDelegate(PasswordAuthenticationDelegate delegate) {
        sDelegate = delegate;
    }

    /**
     * @return Whether password authentication is enabled.
     */
    public static boolean isPasswordAuthenticationEnabled() {
        return getDelegate().isPasswordAuthenticationEnabled();
    }

    /**
     * Requests password authentication be presented for the given tab.
     * @param tab The tab containing the protected password.
     * @param callback The callback to be triggered on authentication result.
     */
    @CalledByNative
    public static void requestAuthentication(
TabBase tab,
            PasswordAuthenticationCallback callback, String[] usernames,
            boolean hideMultiUserScreen) {

        Log.v(TAG, "Request authentication Password AuthenticationManager.java");
        getDelegate().requestAuthentication(tab, callback, usernames,
                hideMultiUserScreen);
    }

    @CalledByNative
    public static void showAutoCompleteAlertPopUp() {
        getDelegate().showAutoCompleteAlertPopUp();
    }

    /**
     * @return Status of WebSigIn Setting
     */
    @CalledByNative
    public static boolean isWebSignInEnabled() {
        return getDelegate().isWebSignInEnabled();
    }

    /**
     * @return The message to be displayed in the save password infobar that will allow the user
     *         to opt-in to additional password authentication.
     */
    public static String getPasswordProtectionString() {
        return getDelegate().getPasswordProtectionString();
    }

    private static native void nativeOnResult(long callbackPtr, boolean authenticated,String selectedUser);
}
