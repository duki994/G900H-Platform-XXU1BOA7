// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.os.AsyncTask;
import android.security.KeyChain;
import android.security.KeyChainAliasCallback;
import android.security.KeyChainException;
import android.util.Log;

import org.chromium.base.ActivityStatus;
import org.chromium.base.CalledByNative;
import org.chromium.base.JNINamespace;
import org.chromium.base.ThreadUtils;
import org.chromium.net.AndroidPrivateKey;
import org.chromium.net.DefaultAndroidKeyStore;

import java.security.Principal;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;

import javax.security.auth.x500.X500Principal;

/**
 * Handles selection of client certificate on the Java side. This class is responsible for selection
 * of the client certificate to be used for authentication and retrieval of the private key and full
 * certificate chain.
 *
 * The entry point is selectClientCertificate() and it will be called on the UI thread. Then the
 * class will construct and run an appropriate CertAsyncTask, that will run in background, and
 * finally pass the results back to the UI thread, which will return to the native code.
 */
@JNINamespace("chrome::android")
public class SSLClientCertificateRequest {
    static final String TAG = "SSLClientCertificateRequest";

    private static final DefaultAndroidKeyStore sLocalKeyStore = new DefaultAndroidKeyStore();

    /**
     * Common implementation for anynchronous task of handling the certificate request. This
     * AsyncTask uses the abstract methods to retrieve the authentication material from a
     * generalized key store. The key store is accessed in background, as the APIs being exercised
     * may be blocking. The results are posted back to native on the UI thread.
     */
    abstract static class CertAsyncTask extends AsyncTask<Void, Void, Void> {
        // These fields will store the results computed in doInBackground so that they can be posted
        // back in onPostExecute.
        private byte[][] mEncodedChain;
        private AndroidPrivateKey mAndroidPrivateKey;

        // Pointer to the native certificate request needed to return the results.
        private final int mNativePtr;

        CertAsyncTask(int nativePtr) {
            mNativePtr = nativePtr;
        }

        // These overriden methods will be used to access the key store.
        abstract String getAlias();
        abstract AndroidPrivateKey getPrivateKey(String alias);
        abstract X509Certificate[] getCertificateChain(String alias);

        @Override
        protected Void doInBackground(Void... params) {
            String alias = getAlias();
            if (alias == null) return null;

            AndroidPrivateKey key = getPrivateKey(alias);
            X509Certificate[] chain = getCertificateChain(alias);

            if (key == null || chain == null || chain.length == 0) {
                Log.w(TAG, "Empty client certificate chain?");
                return null;
            }

            // Encode the certificate chain.
            byte[][] encodedChain = new byte[chain.length][];
            try {
                for (int i = 0; i < chain.length; ++i) {
                    encodedChain[i] = chain[i].getEncoded();
                }
            } catch (CertificateEncodingException e) {
                Log.w(TAG, "Could not retrieve encoded certificate chain: " + e);
                return null;
            }

            mEncodedChain = encodedChain;
            mAndroidPrivateKey = key;
            return null;
        }

        @Override
        protected void onPostExecute(Void result) {
            ThreadUtils.assertOnUiThread();
            nativeOnSystemRequestCompletion(mNativePtr, mEncodedChain, mAndroidPrivateKey);
        }
    }

    /** Implementation of CertAsyncTask for the system KeyChain API. */
    private static class CertAsyncTaskKeyChain extends CertAsyncTask {
        final Context mContext;
        final String mAlias;

        CertAsyncTaskKeyChain(Context context, int nativePtr, String alias) {
            super(nativePtr);
            mContext = context;
            assert alias != null;
            mAlias = alias;
        }

        @Override
        String getAlias() {
            return mAlias;
        }

        @Override
        AndroidPrivateKey getPrivateKey(String alias) {
            try {
                return sLocalKeyStore.createKey(KeyChain.getPrivateKey(mContext, alias));
            } catch (KeyChainException e) {
                Log.w(TAG, "KeyChainException when looking for '" + alias + "' certificate");
                return null;
            } catch (InterruptedException e) {
                Log.w(TAG, "InterruptedException when looking for '" + alias + "'certificate");
                return null;
            }
        }

        @Override
        X509Certificate[] getCertificateChain(String alias) {
            try {
                return KeyChain.getCertificateChain(mContext, alias);
            } catch (KeyChainException e) {
                Log.w(TAG, "KeyChainException when looking for '" + alias + "' certificate");
                return null;
            } catch (InterruptedException e) {
                Log.w(TAG, "InterruptedException when looking for '" + alias + "'certificate");
                return null;
            }
        }
    }

    /** Implementation of CertAsyncTask for use with a PKCS11-backed KeyStore. */
    private static class CertAsyncTaskPKCS11 extends CertAsyncTask {
        private final PKCS11AuthenticationManager mPKCS11AuthManager;
        private final String mHostName;
        private final int mPort;

        CertAsyncTaskPKCS11(int nativePtr, String hostName, int port,
                PKCS11AuthenticationManager pkcs11CardAuthManager) {
            super(nativePtr);
            mHostName = hostName;
            mPort = port;
            mPKCS11AuthManager = pkcs11CardAuthManager;
        }

        @Override
        String getAlias() {
            return mPKCS11AuthManager.getClientCertificateAlias(mHostName, mPort);
        }

        @Override
        AndroidPrivateKey getPrivateKey(String alias) {
            return mPKCS11AuthManager.getPrivateKey(alias);
        }

        @Override
        X509Certificate[] getCertificateChain(String alias) {
            return mPKCS11AuthManager.getCertificateChain(alias);
        }
    }

    /**
     * The system KeyChain API will call us back on the alias() method, passing the alias of the
     * certificate selected by the user.
     */
    private static class KeyChainCertSelectionCallback implements KeyChainAliasCallback {
        private final int mNativePtr;
        private final Context mContext;

        KeyChainCertSelectionCallback(Context context, int nativePtr) {
            mContext = context;
            mNativePtr = nativePtr;
        }

        @Override
        public void alias(final String alias) {
            // This is called by KeyChainActivity in a background thread. Post task to
            // handle the certificate selection on the UI thread.
            ThreadUtils.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    if (alias == null) {
                        // No certificate was selected.
                        ThreadUtils.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                nativeOnSystemRequestCompletion(mNativePtr, null, null);
                            }
                        });
                    } else {
                        new CertAsyncTaskKeyChain(mContext, mNativePtr, alias).execute();
                    }
                }
            });
        }
    }

    /**
     * Create a new asynchronous request to select a client certificate.
     *
     * @param nativePtr The native object responsible for this request.
     * @param keyTypes The list of supported key exchange types.
     * @param encodedPrincipals The list of CA DistinguishedNames.
     * @param hostName The server host name is available (empty otherwise).
     * @param port The server port if available (0 otherwise).
     * @return true on success.
     * Note that nativeOnSystemRequestComplete will be called iff this method returns true.
     */
    @CalledByNative
    private static boolean selectClientCertificate(final int nativePtr, final String[] keyTypes,
            byte[][] encodedPrincipals, final String hostName, final int port) {
        ThreadUtils.assertOnUiThread();
        final Activity activity = ActivityStatus.getActivity();
        if (activity == null) {
            Log.w(TAG, "No active Chromium main activity!?");
            return false;
        }
        // smart card helper might not be ready.
        // call getPKCS11AuthenticationManager earlier to give a time to initialize()
        final Context appContext = activity.getApplicationContext();
        final PKCS11AuthenticationManager smartCardAuthManager =
            ((ChromiumApplication) appContext).getPKCS11AuthenticationManager();

        // Build the list of principals from encoded versions.
        Principal[] principals = null;
        if (encodedPrincipals.length > 0) {
            principals = new X500Principal[encodedPrincipals.length];
            try {
                for (int n = 0; n < encodedPrincipals.length; n++) {
                    principals[n] = new X500Principal(encodedPrincipals[n]);
                }
            } catch (NullPointerException e1) {
                Log.w(TAG, "Exception while decoding issuers list: " + e1.getMessage());
                return false;
            }catch (IllegalArgumentException e2) {
                Log.w(TAG, "Exception while decoding issuers list: " + e2.getMessage());
                return false;
            }
        }

        final Principal[] principalsForCallback = principals;
        // Certificate for client authentication can be obtained either from the system store of
        // from a smart card (if available).
        Runnable useSystemStore = new Runnable() {
            @Override
            public void run() {
                KeyChainCertSelectionCallback callback =
                        new KeyChainCertSelectionCallback(activity.getApplicationContext(), nativePtr);
                try {
                    KeyChain.choosePrivateKeyAlias(activity, callback, keyTypes, principalsForCallback, hostName, port, null);
                } catch (ActivityNotFoundException e) {
                    Log.e(TAG, "Can't start KeyChain. use null alias");
                    callback.alias(null);
                }
            }
        };

        //final Context appContext = activity.getApplicationContext();
        //final PKCS11AuthenticationManager smartCardAuthManager =
        //    ((ChromiumApplication) appContext).getPKCS11AuthenticationManager();
        if (smartCardAuthManager.isPKCS11AuthEnabled()) {
            // Smart card support is available, prompt the user whether to use it or Android system store.
            Runnable useSmartCard = new Runnable() {
                @Override
                public void run() {
                    new CertAsyncTaskPKCS11(nativePtr, hostName, port,
                            smartCardAuthManager).execute();
                }
            };
            Runnable cancelRunnable = new Runnable() {
                @Override
                public void run() {
                    // We took ownership of the request, need to delete it.
                    nativeOnSystemRequestCompletion(nativePtr, null, null);
                }
            };

            KeyStoreSelectionDialog selectionDialog = 
                    new KeyStoreSelectionDialog(useSystemStore, useSmartCard, cancelRunnable);
            
            if(ActivityStatus.isApplicationVisible()) {
                selectionDialog.show(activity.getFragmentManager(), null);
            } else { 
                nativeOnSystemRequestCompletion(nativePtr, null, null);
            }
        } else {
            // Smart card support is not available, use the system store unconditionally.
            useSystemStore.run();
        }

        // We've taken ownership of the native ssl request object.
        return true;
    }

    // TODO(yfriedman): Java code doesn't have a global for the IO thread so it's exposed here.
    // X509Util helper function could probably move here (as it's still in net/)
    public static void notifyClientCertificatesChangedOnIOThread() {
        Log.d(TAG, "ClientCertificatesChanged!");
        nativeNotifyClientCertificatesChangedOnIOThread();
    }

    private static native void nativeNotifyClientCertificatesChangedOnIOThread();

    // Called to pass request results to native side.
    private static native void nativeOnSystemRequestCompletion(
            int requestPtr, byte[][] certChain, AndroidPrivateKey androidKey);
}
